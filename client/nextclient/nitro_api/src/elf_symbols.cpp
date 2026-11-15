#include "elf_symbols.h"

#include <cstring>
#include <limits>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sys/sysmacros.h>
#include <dlfcn.h>
#include <elf.h>
#include <fstream>
#include <link.h>
#include <unordered_map>
#include <vector>

namespace nitroapi
{
    namespace
    {
        using SymbolTable = std::unordered_map<std::string, uintptr_t>;

        // Reads the full .symtab from the file on disk - unlike dlsym, this also
        // sees static (file-local) symbols. The records are the ELF class of this
        // process; a cross-architecture file cannot bind a native module.
        SymbolTable ReadElfSymbolTable(const char* path)
        {
            SymbolTable table;

            std::ifstream file(path, std::ios::binary);
            if (!file)
                return table;

            ElfW(Ehdr) header{};
            file.read(reinterpret_cast<char*>(&header), sizeof(header));
            const unsigned char native_class = sizeof(void*) == 8 ? ELFCLASS64 : ELFCLASS32;
            if (!file || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != native_class)
                return table;

            std::vector<ElfW(Shdr)> sections(header.e_shnum);
            file.seekg(header.e_shoff);
            file.read(reinterpret_cast<char*>(sections.data()), sections.size() * sizeof(ElfW(Shdr)));
            if (!file)
                return table;

            for (const ElfW(Shdr)& section : sections)
            {
                if (section.sh_type != SHT_SYMTAB || section.sh_link >= sections.size())
                    continue;

                std::vector<ElfW(Sym)> symbols(section.sh_size / sizeof(ElfW(Sym)));
                file.seekg(section.sh_offset);
                file.read(reinterpret_cast<char*>(symbols.data()), symbols.size() * sizeof(ElfW(Sym)));

                const ElfW(Shdr)& strtab_section = sections[section.sh_link];
                std::vector<char> strtab(strtab_section.sh_size + 1, '\0');
                file.seekg(strtab_section.sh_offset);
                file.read(strtab.data(), strtab_section.sh_size);

                if (!file)
                    return table;

                for (const ElfW(Sym)& symbol : symbols)
                {
                    if (symbol.st_value == 0 || symbol.st_shndx == SHN_UNDEF || symbol.st_name >= strtab_section.sh_size)
                        continue;

                    table.try_emplace(&strtab[symbol.st_name], static_cast<uintptr_t>(symbol.st_value));
                }
            }

            return table;
        }

    }

    uintptr_t FindSymbolAddress(void* module, const std::string& name)
    {
        link_map* linkmap = nullptr;
        if (module == nullptr || dlinfo(module, RTLD_DI_LINKMAP, &linkmap) != 0 || linkmap == nullptr)
            return 0;

        static std::unordered_map<std::string, SymbolTable> tables;
        auto [it, inserted] = tables.try_emplace(linkmap->l_name);
        if (inserted)
            it->second = ReadElfSymbolTable(linkmap->l_name);

        auto symbol = it->second.find(name);
        if (symbol == it->second.end())
            return 0;

        return static_cast<uintptr_t>(linkmap->l_addr) + symbol->second;
    }
}


namespace nitroapi
{
    namespace
    {
        constexpr uint64_t metadata_limit = 32 * 1024 * 1024;
        bool Within(uint64_t offset, uint64_t size, uint64_t limit)
        {
            return offset <= limit && size <= limit - offset;
        }
        struct NativeFile
        {
            int descriptor{-1};
            uint64_t size{};
            struct stat identity{};
            explicit NativeFile(const char* path) : descriptor(open(path, O_RDONLY | O_CLOEXEC))
            {
                if (descriptor >= 0 && fstat(descriptor, &identity) == 0 &&
                    S_ISREG(identity.st_mode) && identity.st_size > 0)
                    size = static_cast<uint64_t>(identity.st_size);
            }
            ~NativeFile() { if (descriptor >= 0) close(descriptor); }
            bool Read(uint64_t offset, void* data, size_t count)
            {
                if (descriptor < 0 || !Within(offset, count, size) || count > metadata_limit ||
                    offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) return false;
                size_t done = 0;
                while (done < count)
                {
                    const auto amount = pread(descriptor, static_cast<char*>(data) + done,
                                              count - done, static_cast<off_t>(offset + done));
                    if (amount < 0 && errno == EINTR) continue;
                    if (amount <= 0) return false;
                    done += static_cast<size_t>(amount);
                }
                return true;
            }
        };
        bool NoteId(const unsigned char* data, size_t size, std::vector<unsigned char>& id)
        {
            size_t offset = 0;
            while (offset < size)
            {
                if (!Within(offset, sizeof(ElfW(Nhdr)), size)) return false;
                ElfW(Nhdr) note{};
                std::memcpy(&note, data + offset, sizeof(note));
                offset += sizeof(note);
                const uint64_t names = (uint64_t(note.n_namesz) + 3) & ~uint64_t(3);
                const uint64_t desc = (uint64_t(note.n_descsz) + 3) & ~uint64_t(3);
                if (!Within(offset, names, size) || !Within(offset + names, desc, size)) return false;
                if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 &&
                    std::memcmp(data + offset, "GNU", 4) == 0)
                {
                    if (!id.empty() || note.n_descsz == 0 || note.n_descsz > 64) return false;
                    id.assign(data + offset + names, data + offset + names + note.n_descsz);
                }
                offset += static_cast<size_t>(names + desc);
            }
            return true;
        }
        struct LoadedObject
        {
            uintptr_t base{};
            std::vector<ElfW(Phdr)> headers;
        };
        int LoadedHeaders(dl_phdr_info* info, size_t, void* opaque)
        {
            auto& object = *static_cast<LoadedObject*>(opaque);
            if (info->dlpi_addr != object.base) return 0;
            if (info->dlpi_phnum > 0 && info->dlpi_phnum <= 1024)
                object.headers.assign(info->dlpi_phdr, info->dlpi_phdr + info->dlpi_phnum);
            return 1;
        }
        bool LoadedRange(const LoadedObject& object, uint64_t address, uint64_t size, unsigned flags)
        {
            for (const auto& segment : object.headers)
                if (segment.p_type == PT_LOAD && (segment.p_flags & flags) == flags &&
                    address >= segment.p_vaddr && Within(address - segment.p_vaddr, size, segment.p_memsz))
                    return true;
            return false;
        }
        bool ExecutableMapping(uintptr_t address, size_t size, const struct stat& disk)
        {
            std::ifstream maps("/proc/self/maps");
            std::string line;
            while (std::getline(maps, line))
            {
                std::istringstream fields(line);
                uint64_t start{}, end{}, offset{}, inode{};
                unsigned device_major{}, device_minor{};
                char dash{}, colon{};
                std::string permissions;
                if (!(fields >> std::hex >> start >> dash >> end >> permissions >> offset >>
                      device_major >> colon >> device_minor >> std::dec >> inode) || dash != '-' || colon != ':') continue;
                if (permissions.size() != 4 || permissions[2] != 'x' || inode != disk.st_ino ||
                    device_major != major(disk.st_dev) || device_minor != minor(disk.st_dev)) continue;
                if (address >= start && end >= start && Within(address - start, size, end - start)) return true;
            }
            return false;
        }
    }

    uintptr_t FindFunctionAddress(void* module, const std::string& name)
    {
        link_map* map = nullptr;
        if (!module || name.empty() || dlinfo(module, RTLD_DI_LINKMAP, &map) != 0 || !map ||
            !map->l_name || !*map->l_name) return 0;
        NativeFile file(map->l_name);
        const auto& disk = file.identity;
        if (!file.size) return 0;
        ElfW(Ehdr) header{};
        if (!file.Read(0, &header, sizeof(header)) || std::memcmp(header.e_ident, ELFMAG, SELFMAG) ||
            header.e_ident[EI_CLASS] != (sizeof(void*) == 8 ? ELFCLASS64 : ELFCLASS32) ||
            header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_ident[EI_VERSION] != EV_CURRENT ||
            header.e_machine != (sizeof(void*) == 8 ? EM_X86_64 : EM_386) ||
            header.e_version != EV_CURRENT || header.e_type != ET_DYN ||
            header.e_ehsize != sizeof(header) || header.e_phentsize != sizeof(ElfW(Phdr)) ||
            header.e_shentsize != sizeof(ElfW(Shdr)) || !header.e_phnum || header.e_phnum > 1024 ||
            !header.e_shnum || header.e_shnum > 8192) return 0;
        LoadedObject loaded{static_cast<uintptr_t>(map->l_addr), {}};
        dl_iterate_phdr(LoadedHeaders, &loaded);
        if (loaded.headers.empty()) return 0;
        std::vector<unsigned char> memory_id, disk_id;
        for (const auto& segment : loaded.headers)
        {
            if (segment.p_type != PT_NOTE) continue;
            if (segment.p_filesz > metadata_limit || !LoadedRange(loaded, segment.p_vaddr, segment.p_filesz, PF_R) ||
                segment.p_vaddr > UINTPTR_MAX - loaded.base ||
                !NoteId(reinterpret_cast<const unsigned char*>(loaded.base + segment.p_vaddr), segment.p_filesz, memory_id)) return 0;
        }
        std::vector<ElfW(Phdr)> programs(header.e_phnum);
        if (!file.Read(header.e_phoff, programs.data(), programs.size() * sizeof(programs[0]))) return 0;
        for (const auto& segment : programs)
        {
            if (!Within(segment.p_offset, segment.p_filesz, file.size) ||
                (segment.p_type == PT_LOAD && segment.p_filesz > segment.p_memsz)) return 0;
            if (segment.p_type != PT_NOTE) continue;
            if (segment.p_filesz > metadata_limit) return 0;
            std::vector<unsigned char> notes(segment.p_filesz);
            if (!file.Read(segment.p_offset, notes.data(), notes.size()) || !NoteId(notes.data(), notes.size(), disk_id)) return 0;
        }
        if (memory_id.empty() || memory_id != disk_id) return 0;
        std::vector<ElfW(Shdr)> sections(header.e_shnum);
        if (!file.Read(header.e_shoff, sections.data(), sections.size() * sizeof(sections[0]))) return 0;
        uintptr_t result = 0;
        for (const auto& section : sections)
        {
            if (section.sh_type != SHT_SYMTAB && section.sh_type != SHT_DYNSYM) continue;
            if (section.sh_entsize != sizeof(ElfW(Sym)) || section.sh_size % sizeof(ElfW(Sym)) ||
                section.sh_size > metadata_limit || section.sh_link >= sections.size()) return 0;
            const auto& strings = sections[section.sh_link];
            if (strings.sh_type != SHT_STRTAB || strings.sh_size > metadata_limit) return 0;
            std::vector<char> names(strings.sh_size);
            std::vector<ElfW(Sym)> symbols(section.sh_size / sizeof(ElfW(Sym)));
            if (!file.Read(strings.sh_offset, names.data(), names.size()) ||
                !file.Read(section.sh_offset, symbols.data(), section.sh_size)) return 0;
            for (const auto& symbol : symbols)
            {
                if (symbol.st_name >= names.size()) return 0;
                const char* candidate = names.data() + symbol.st_name;
                const auto* end = static_cast<const char*>(std::memchr(candidate, 0, names.size() - symbol.st_name));
                if (!end) return 0;
                if (name.size() != static_cast<size_t>(end - candidate) || name.compare(candidate) != 0) continue;
                if (ELF64_ST_TYPE(symbol.st_info) != STT_FUNC || !symbol.st_size || !symbol.st_value ||
                    symbol.st_shndx == SHN_UNDEF || symbol.st_shndx >= sections.size() ||
                    symbol.st_value > UINTPTR_MAX - loaded.base ||
                    !LoadedRange(loaded, symbol.st_value, symbol.st_size, PF_X)) return 0;
                bool disk_executable = false;
                for (const auto& segment : programs)
                    if (segment.p_type == PT_LOAD && (segment.p_flags & PF_X) && symbol.st_value >= segment.p_vaddr &&
                        Within(symbol.st_value - segment.p_vaddr, symbol.st_size, segment.p_filesz)) disk_executable = true;
                const uintptr_t address = loaded.base + symbol.st_value;
                Dl_info owner{};
                if (!disk_executable || !ExecutableMapping(address, symbol.st_size, disk) || !dladdr(reinterpret_cast<void*>(address), &owner) ||
                    reinterpret_cast<uintptr_t>(owner.dli_fbase) != loaded.base || (result && result != address)) return 0;
                result = address;
            }
        }
        return result;
    }
}
