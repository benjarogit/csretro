#include "client_callbacks.h"
#include "build_registry.h"
#include "vgui_binding.h"
#include <xash_surface/xash_surface.h>

#include <array>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <memory>

extern "C" __attribute__((visibility("default"))) void F(goldsrc::cldll_func_t* out);

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;
    int BridgeInitialize(goldsrc::cl_enginefunc_t*, int);
    void BridgeShutdown();

    class OriginalUi
    {
    public:
        void Shutdown() { binding_.Shutdown(); }
        ~OriginalUi()
        {
            binding_.Shutdown();
            vgui_.reset();
            if (surface_) destroy_(surface_);
        }

        bool Initialize(void* client, const std::filesystem::path& directory,
                        const std::string& client_hash, goldsrc::cl_enginefunc_t* engine,
                        std::string& error)
        {
            if (binding_.IsInitialized())
                return true;
            if (surface_ && vgui_)
                return binding_.Initialize(client, vgui_.get(), surface_object_, error);
            const auto vgui_path = directory / "vgui2.valve.so";
            if (!nitroapi::IsRegisteredClientVgui(client_hash, nitroapi::HashFileSha256(vgui_path.string())))
            {
                error = "vgui2.valve.so does not match the registered original client";
                return false;
            }
            goldsrc::SCREENINFO screen{};
            screen.iSize = sizeof(screen);
            if (!engine || !engine->pfnGetScreenInfo || !engine->pfnGetScreenInfo(&screen) ||
                screen.iWidth <= 0 || screen.iHeight <= 0)
            {
                error = "Xash has not supplied a valid screen size";
                return false;
            }
            surface_module_.reset(dlopen((directory / "xash_surface.so").c_str(), RTLD_NOW | RTLD_LOCAL));
            if (!surface_module_)
            {
                const char* reason = dlerror();
                error = reason ? reason : "Cannot load xash_surface.so";
                return false;
            }
            auto create = reinterpret_cast<csr_surface_t* (*)(const csr_surface_host_t*)>(
                    dlsym(surface_module_.get(), "csr_surface_create"));
            auto object = reinterpret_cast<void* (*)(csr_surface_t*)>(
                    dlsym(surface_module_.get(), "csr_surface_vgui"));
            destroy_ = reinterpret_cast<void (*)(csr_surface_t*)>(dlsym(surface_module_.get(), "csr_surface_destroy"));
            if (!create || !object || !destroy_)
            {
                error = "Surface module has an incomplete host API";
                return false;
            }
            csr_surface_host_t host{};
            host.screen_wide = screen.iWidth;
            host.screen_tall = screen.iHeight;
            surface_ = create(&host);
            if (!surface_)
            {
                error = "Cannot create the single product Surface";
                return false;
            }
            vgui_.reset(dlopen(vgui_path.c_str(), RTLD_NOW | RTLD_LOCAL));
            if (!vgui_)
            {
                const char* reason = dlerror();
                error = reason ? reason : "Cannot load vgui2.valve.so";
                return false;
            }
            surface_object_ = object(surface_);
            return binding_.Initialize(client, vgui_.get(), surface_object_, error);
        }

    private:
        Module surface_module_{nullptr, &dlclose};
        Module vgui_{nullptr, &dlclose};
        csr_surface_t* surface_{};
        void* surface_object_{};
        void (*destroy_)(csr_surface_t*){};
        csr::client::VguiBinding binding_;
    };

    class OriginalClient
    {
    public:
        ~OriginalClient()
        {
            if (ui_) ui_->Shutdown();
            // The original client's static destructors can still reference
            // VGUI code. Unload it while both UI modules and Surface are live.
            module_.reset();
            ui_.reset();
        }
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
            const auto directory = std::filesystem::canonical(self.dli_fname).parent_path();
            const auto path = directory / "client.valve.so";
            std::array<unsigned char, 20> header{};
            std::ifstream file(path, std::ios::binary);
            if (!file.read(reinterpret_cast<char*>(header.data()), header.size()) ||
                header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F' ||
                header[4] != 1 || header[5] != 1 || header[18] != 3 || header[19] != 0)
            {
                error = "client.valve.so is missing or is not an i386 ELF library";
                return false;
            }
            const auto client_hash = nitroapi::HashFileSha256(path.string());
            if (!nitroapi::IsRegisteredClient("linux", client_hash))
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
            directory_ = directory;
            client_hash_ = client_hash;
            module_ = std::move(candidate);
            return true;
        }

        goldsrc::cldll_func_t Callbacks() const
        {
            auto table = callbacks_;
            table.pInitFunc = BridgeInitialize;
            table.pShutdown = BridgeShutdown;
            return table;
        }

        int Initialize(goldsrc::cl_enginefunc_t* engine, int version)
        {
            return csr::client::InitializeWithVgui(callbacks_.pInitFunc, engine, version,
                    [](void* owner, goldsrc::cl_enginefunc_t* funcs) {
                        return static_cast<OriginalClient*>(owner)->BindUi(funcs);
                    }, this);
        }

        bool BindUi(goldsrc::cl_enginefunc_t* engine)
        {
            auto candidate = ui_ ? nullptr : std::make_unique<OriginalUi>();
            auto* ui = ui_ ? ui_.get() : candidate.get();
            std::string error;
            if (!ui->Initialize(module_.get(), directory_, client_hash_, engine, error))
            {
                std::fprintf(stderr, "CSRETRO client bridge: %s\n", error.c_str());
                return false;
            }
            if (candidate) ui_ = std::move(candidate);
            return true;
        }

        void Shutdown()
        {
            callbacks_.pShutdown();
            if (ui_) ui_->Shutdown();
        }

    private:
        // Xash calls original shutdown before unloading this bridge. Retain
        // the game module until then; no callback may outlive its code.
        Module module_{nullptr, &dlclose};
        goldsrc::cldll_func_t callbacks_{};
        std::filesystem::path directory_;
        std::string client_hash_;
        std::unique_ptr<OriginalUi> ui_;
    };

    OriginalClient& Original() { static OriginalClient client; return client; }
    int BridgeInitialize(goldsrc::cl_enginefunc_t* engine, int version)
    {
        try { return Original().Initialize(engine, version); }
        catch (const std::exception& exception)
        {
            std::fprintf(stderr, "CSRETRO client bridge: %s\n", exception.what());
        }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: initialization failed\n"); }
        return 0;
    }
    void BridgeShutdown()
    {
        try { Original().Shutdown(); }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: shutdown failed\n"); }
    }
}

extern "C" __attribute__((visibility("default"))) void F(goldsrc::cldll_func_t* out)
{
    if (!out)
        return;
    *out = {};
    try
    {
        auto& original = Original();
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
