#pragma once

#include <string>
#include <goldsrc/client_api.h>

namespace csr::client
{
    // The caller owns the module and must retain it until all callbacks finish.
    // This reads the original F table; it supplies no substitute callbacks.
    bool ReadCallbacks(void* module, goldsrc::cldll_func_t& out, std::string& error);
}
