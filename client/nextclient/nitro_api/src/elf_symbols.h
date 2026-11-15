#pragma once

#include <cstdint>
#include <string>

namespace nitroapi
{
    // Resolve a file-local symbol in an already loaded, borrowed native ELF
    // module. No library is opened or unloaded and no fixed address is used.
    uintptr_t FindSymbolAddress(void* module, const std::string& name);

    // Resolve a defined native function only when fresh file metadata matches
    // the loaded GNU build ID and its complete range is executable in this object.
    uintptr_t FindFunctionAddress(void* module, const std::string& name);
}
