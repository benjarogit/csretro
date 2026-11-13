#include "elf_symbols.h"

#include <cstring>
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
