#include <cstdint>
#include "SearchConfig.h"

#include <utility>
#include <iostream>

#ifndef _WIN32
    #include "elf_symbols.h"
#endif

namespace nitroapi
{
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