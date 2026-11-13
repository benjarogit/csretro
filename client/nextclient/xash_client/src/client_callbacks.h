#pragma once

#include <string>
#include <goldsrc/client_api.h>

namespace csr::client
{
    // The caller owns the module and must retain it until all callbacks finish.
    // This reads the original F table; it supplies no substitute callbacks.
    bool ReadCallbacks(void* module, goldsrc::cldll_func_t& out, std::string& error);

    using BindVgui = bool (*)(void* owner, goldsrc::cl_enginefunc_t* engine);
    int InitializeWithVgui(goldsrc::INITIALIZE_FUNC original, goldsrc::cl_enginefunc_t* engine,
                           int version, BindVgui bind, void* owner);
}
