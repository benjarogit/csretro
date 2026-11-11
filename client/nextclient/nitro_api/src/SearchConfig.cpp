#include <cstdint>
#include "SearchConfig.h"

#include <utility>
#include <iostream>

#ifndef _WIN32
    #include <dlfcn.h>
    #include <cstring>
    #include <elf.h>
    #include <link.h>
    #include <fstream>
    #include <unordered_map>
    #include <vector>
#endif

namespace nitroapi
{
#ifndef _WIN32
    namespace
    {
        using SymbolTable = std::unordered_map<std::string, uintptr_t>;

        // Reads the full .symtab from the file on disk - unlike dlsym, this also
        // sees static (file-local) symbols. The records are the ELF class of this
        // process. On-disk GoldSrc files are not parsed here.
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

        uintptr_t FindSymbolAddress(nitro_utils::SysModule module, const std::string& name)
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
#endif

    SearchConfig::SearchConfig(std::string search_string, SearchType search_type) :
        search_string(std::move(search_string)),
        search_type(search_type),
        offset(0)
    { }

    SearchConfig::SearchConfig(uintptr_t offset) :
        search_string(),
        search_type(SearchType::Offset),
        offset(offset)
    { }

    SearchConfig::SearchConfig(std::string search_string, uintptr_t offset, SearchType search_type) :
        search_string(std::move(search_string)),
        offset(offset),
        search_type(search_type)
    { }

    SearchConfig::SearchConfig(std::string search_string, SearchType search_type, std::string fallback_pattern, uintptr_t offset) :
        search_string(std::move(search_string)),
        fallback_pattern(std::move(fallback_pattern)),
        offset(offset),
        search_type(search_type)
    { }

    uintptr_t SearchConfig::FindAddress(MemScanner& mem_scanner) const
    {
        switch (search_type)
        {
            default:
            case SearchType::Pattern:
                return mem_scanner.FindPattern(search_string.c_str());

            case SearchType::PatternEx:
                return mem_scanner.FindPattern2(search_string);

            case SearchType::ExportFunc:
                return reinterpret_cast<uintptr_t>(nitro_utils::GetProcAddress(mem_scanner.Module(), search_string.c_str()));

            case SearchType::Symbol:
#ifdef _WIN32
                return 0;
#else
                return FindSymbolAddress(mem_scanner.Module(), search_string);
#endif

            case SearchType::SymbolOrPattern:
            {
                uintptr_t address = reinterpret_cast<uintptr_t>(
                    nitro_utils::GetProcAddress(mem_scanner.Module(), search_string.c_str()));

                if (address != 0)
                    return address;

#ifndef _WIN32
                address = FindSymbolAddress(mem_scanner.Module(), search_string);

                if (address != 0)
                    return address;
#endif

                if (fallback_pattern.empty())
                    return 0;

                const uintptr_t pattern_address = mem_scanner.FindPattern(fallback_pattern.c_str());

                if (pattern_address == 0)
                    return 0;

                return pattern_address + offset;
            }

            case SearchType::Offset:
                return mem_scanner.ModuleStart() + offset;

            case SearchType::PatternAndOffset:
                return mem_scanner.FindPattern(search_string.c_str()) + offset;

            case SearchType::PatternExAndOffset:
                return mem_scanner.FindPattern2(search_string) + offset;

            case SearchType::String:
                return mem_scanner.FindString(search_string.c_str());

            case SearchType::NotPresent:
                return 0;
        }
    }
}