#include "client_callbacks.h"
#include "build_registry.h"

#include <array>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <memory>

extern "C" __attribute__((visibility("default"))) void F(goldsrc::cldll_func_t* out);

namespace
{
    class OriginalClient
    {
    public:
        bool Load(std::string& error)
        {
            if (module_)
                return true;
            if (sizeof(void*) != 4)
            {
                error = "The original Public game client requires the i386 port profile";
                return false;
            }
            Dl_info self{};
            if (!dladdr(reinterpret_cast<void*>(&F), &self) || !self.dli_fname)
            {
                error = "Cannot locate the client bridge";
                return false;
            }
            const auto path = std::filesystem::path(self.dli_fname).parent_path() / "client.valve.so";
            std::array<unsigned char, 20> header{};
            std::ifstream file(path, std::ios::binary);
            if (!file.read(reinterpret_cast<char*>(header.data()), header.size()) ||
                header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F' ||
                header[4] != 1 || header[5] != 1 || header[18] != 3 || header[19] != 0)
            {
                error = "client.valve.so is missing or is not an i386 ELF library";
                return false;
            }
            if (!nitroapi::IsRegisteredClient("linux", nitroapi::HashFileSha256(path.string())))
            {
                error = "client.valve.so does not match the registered Default Public game client";
                return false;
            }
            Module candidate(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL), &dlclose);
            if (!candidate)
            {
                const char* reason = dlerror();
                error = reason ? reason : "Cannot load client.valve.so";
                return false;
            }
            if (!csr::client::ReadCallbacks(candidate.get(), callbacks_, error))
                return false;
            module_ = std::move(candidate);
            return true;
        }

        const goldsrc::cldll_func_t& Callbacks() const { return callbacks_; }

    private:
        // Xash calls original shutdown before unloading this bridge. Retain
        // the game module until then; no callback may outlive its code.
        using Module = std::unique_ptr<void, int (*)(void*)>;
        Module module_{nullptr, &dlclose};
        goldsrc::cldll_func_t callbacks_{};
    };
}

extern "C" __attribute__((visibility("default"))) void F(goldsrc::cldll_func_t* out)
{
    if (!out)
        return;
    *out = {};
    try
    {
        static OriginalClient original;
        std::string error;
        if (original.Load(error))
        {
            *out = original.Callbacks();
            return;
        }
        std::fprintf(stderr, "CSRETRO client bridge: %s\n", error.c_str());
    }
    catch (const std::exception& exception)
    {
        std::fprintf(stderr, "CSRETRO client bridge: %s\n", exception.what());
    }
    catch (...)
    {
        std::fprintf(stderr, "CSRETRO client bridge: callback loading failed\n");
    }
}
