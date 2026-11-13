#pragma once

#include "elf_symbols.h"

namespace csr::client::detail
{
    template <class T>
    T Symbol(void* module, const char* name, std::string& error)
    {
        auto address = nitroapi::FindSymbolAddress(module, name);
        if (!address && error.empty())
            error = std::string("Missing original VGUI binding symbol: ") + name;
        return reinterpret_cast<T>(address);
    }
}
