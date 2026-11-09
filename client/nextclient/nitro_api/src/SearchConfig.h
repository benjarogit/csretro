#pragma once

#include <string>
#include "nitro_utils/MemoryTools.h"

namespace nitroapi
{
    enum class SearchType
    {
        Pattern,
        PatternEx,
        ExportFunc,
        Symbol,
        SymbolOrPattern,
        Offset,
        PatternAndOffset,
        PatternExAndOffset,
        String,
        NotPresent
    };

    struct SearchConfig
    {
        std::string search_string;
        std::string fallback_pattern;
        uint32_t offset{};
        SearchType search_type = SearchType::NotPresent;

        explicit SearchConfig() = default;
        explicit SearchConfig(std::string search_string, SearchType search_type = SearchType::Pattern);
        explicit SearchConfig(uint32_t offset);
        explicit SearchConfig(std::string search_string, uint32_t offset, SearchType search_type = SearchType::PatternAndOffset);
        explicit SearchConfig(std::string search_string, SearchType search_type, std::string fallback_pattern, uint32_t offset = 0);

        [[nodiscard]] uint32_t FindAddress(MemScanner& mem_scanner) const;
    };
}
