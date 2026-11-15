#include "client_callbacks.h"
#include "build_registry.h"
#include "original_ui.h"

#include <array>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <exception>
#include <memory>

extern "C" __attribute__((visibility("default"))) void F(goldsrc::cldll_func_t* out);

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;
    int BridgeInitialize(goldsrc::cl_enginefunc_t*, int);
    void BridgeShutdown();
    int BridgeRedraw(float, int);
    void BridgeFrame(double);
    void BridgeMouse(int);
    void BridgeClearInput();
    void BridgeDeactivateMouse();
    int BridgeKeyEvent(int, int, const char*);

    using OriginalUi = csr::client::OriginalUi;

    class OriginalClient
    {
    public:
        ~OriginalClient()
        {
            try
            {
                Shutdown();
            }
            catch (...)
            {
                std::fprintf(stderr, "CSRETRO original client teardown failed\n");
            }
            if (ui_ && !ui_->CanDestroy())
            {
                // A live callback still owns this graph. Pin bridge code as
                // well as every native module; dangling Surface hosts are unsafe.
                Dl_info self{};
                if (dladdr(reinterpret_cast<void*>(&F), &self) && self.dli_fname)
                {
                    static_cast<void>(dlopen(self.dli_fname, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE));
                }
                static_cast<void>(module_.release());
                static_cast<void>(ui_.release());
                std::fprintf(stderr, "CSRETRO retained incomplete native UI lifetime; restart required\n");
                return;
            }
            // Original static destructors can still reference VGUI/Surface.
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
            table.pHudRedrawFunc = BridgeRedraw;
            table.pHudFrame = BridgeFrame;
            table.pIN_MouseEvent = BridgeMouse;
            table.pIN_ClearStates = BridgeClearInput;
            table.pIN_DeactivateMouse = BridgeDeactivateMouse;
            table.pKeyEvent = BridgeKeyEvent;
            return table;
        }

        int Initialize(goldsrc::cl_enginefunc_t* engine, int version)
        {
            if (!initialization_->Begin())
            {
                return 0;
            }
            if (!ui_)
            {
                ui_ = std::make_unique<OriginalUi>();
            }
            ui_->SetOriginalShutdown([state = initialization_, shutdown = callbacks_.pShutdown] {
                state->Shutdown(shutdown, nullptr, nullptr);
            });
            std::string error;
            int result{};
            try
            {
                if (!ui_->Prepare(engine, client_hash_, error))
                {
                    initialization_->Finish(false);
                    std::fprintf(stderr, "CSRETRO client preflight: %s\n", error.c_str());
                    return 0;
                }
                result = callbacks_.pInitFunc(engine, version);
                initialization_->Finish(result != 0);
                if (!result)
                {
                    return result;
                }
                if (initialization_->ShutdownRequested() || !BindUi(engine))
                {
                    Shutdown();
                    return 0;
                }
                return result;
            }
            catch (...)
            {
                initialization_->Finish(result != 0);
                Shutdown();
                throw;
            }
        }

        bool BindUi(goldsrc::cl_enginefunc_t* engine)
        {
            // Retain partially initialized UI libraries too: if initialization
            // fails, the game module must unload before those dependencies.
            if (!ui_) ui_ = std::make_unique<OriginalUi>();
            std::string error;
            if (!ui_->Initialize(module_.get(), directory_, client_hash_, engine, callbacks_.pFindKey, error))
            {
                std::fprintf(stderr, "CSRETRO client bridge: %s\n", error.c_str());
                return false;
            }
            return true;
        }

        int Redraw(float time, int intermission)
        {
            return csr::client::RedrawWithVgui(callbacks_.pHudRedrawFunc, time, intermission,
                    [](void* owner) { auto& self = *static_cast<OriginalClient*>(owner); if (self.ui_) self.ui_->Paint(); }, this);
        }

        void Frame(double time)
        {
            if (callbacks_.pHudFrame)
            {
                callbacks_.pHudFrame(time);
            }
            if (ui_)
            {
                ui_->Frame(time);
            }
        }

        void Mouse(int buttons)
        {
            csr::client::MouseWithVgui(callbacks_.pIN_MouseEvent, buttons,
                    [](void* owner) { auto& self = *static_cast<OriginalClient*>(owner); if (self.ui_) self.ui_->MouseMove(); },
                    [](void* owner, int state) { auto& self = *static_cast<OriginalClient*>(owner); if (self.ui_) self.ui_->MouseButtons(state); }, this);
        }

        void ClearInput(bool deactivate)
        {
            csr::client::ResetInputWithVgui(
                deactivate ? callbacks_.pIN_DeactivateMouse : callbacks_.pIN_ClearStates,
                [](void* owner) {
                    auto& self = *static_cast<OriginalClient*>(owner);
                    if (self.ui_)
                    {
                        self.ui_->ClearInput();
                    }
                },
                this
            );
        }

        int KeyEvent(int down, int key, const char* binding)
        {
            if (ui_)
            {
                return ui_->KeyEvent(keyboard_, callbacks_.pKeyEvent, down, key, binding);
            }
            return keyboard_.Event(callbacks_.pKeyEvent, down, key, binding, nullptr, nullptr);
        }

        void Shutdown()
        {
            if (ui_ && ui_->HasBinding() && !initialization_->IsInitializing())
            {
                // The binding invokes original shutdown only after its outermost
                // native operation returns, then retires UI while modules live.
                ui_->Shutdown();
                return;
            }
            initialization_->Shutdown(
                callbacks_.pShutdown,
                [](void* owner) {
                    auto& self = *static_cast<OriginalClient*>(owner);
                    if (self.ui_)
                    {
                        self.ui_->Shutdown();
                    }
                },
                this
            );
        }

    private:
        // Xash calls original shutdown before unloading this bridge. Retain
        // the game module until then; no callback may outlive its code.
        std::shared_ptr<csr::client::ClientInitialization> initialization_ = std::make_shared<csr::client::ClientInitialization>();
        Module module_{nullptr, &dlclose};
        goldsrc::cldll_func_t callbacks_{};
        csr::client::NativeKeyCapture keyboard_;
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
    int BridgeRedraw(float time, int intermission)
    {
        try { return Original().Redraw(time, intermission); }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: UI redraw failed\n"); return 0; }
    }
    void BridgeFrame(double time)
    {
        try { Original().Frame(time); }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: UI frame failed\n"); }
    }
    void BridgeMouse(int buttons)
    {
        try { Original().Mouse(buttons); }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: mouse input failed\n"); }
    }
    int BridgeKeyEvent(int down, int key, const char* binding)
    {
        try
        {
            return Original().KeyEvent(down, key, binding);
        }
        catch (...)
        {
            std::fprintf(stderr, "CSRETRO client bridge: key input failed\n");
            return 0;
        }
    }
    void BridgeClearInput()
    {
        try { Original().ClearInput(false); }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: input reset failed\n"); }
    }
    void BridgeDeactivateMouse()
    {
        try { Original().ClearInput(true); }
        catch (...) { std::fprintf(stderr, "CSRETRO client bridge: mouse deactivation failed\n"); }
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
