#pragma once

#include <cstdint>
#include <string>

namespace nitroapi
{
    // Resolve a file-local symbol in an already loaded, borrowed native ELF
    // module. No library is opened or unloaded and no fixed address is used.
    uintptr_t FindSymbolAddress(void* module, const std::string& name);
}
