#pragma once

#include <goldsrc/client_api.h>
#include <memory>
#include <string>

namespace csr::client
{
    // Query layer: its owner must authenticate the engine/profile first.
    // Uses only bounded original legacy key numbers and verified functions.
    class XashKeyQueries
    {
    public:
        bool Bind(
            void* engine,
            void* client,
            void* vgui,
            goldsrc::cl_enginefunc_t* callbacks,
            goldsrc::HUD_KB_FIND_FUNC find,
            int engine_key_count,
            std::string& error
        );
        void Shutdown();
        bool IsBound() const
        {
            return callbacks_ != nullptr;
        }
        bool IsActionDown(const char* action, bool& down) const;
        const char* Name(int key) const;
        const char* Binding(int key) const;
        int Number(const char* name) const;
        int VguiCodeForBinding(const char* command) const;

    private:
        static constexpr int legacy_keys = 256;
        goldsrc::cl_enginefunc_t* callbacks_{};
        goldsrc::HUD_KB_FIND_FUNC find_{};
        const char* (*name_)(int){};
        const char* (*binding_)(int){};
        int (*sdl_key_)(const char*){};
        int (*vgui_key_)(int){};
    };

    // Production owner: only an already-loaded, fingerprinted Xash engine.
    // NOLOAD retains its lifetime; no engine entry point is called to bind.
    class XashKeys
    {
    public:
        bool Bind(void* client, void* vgui, goldsrc::cl_enginefunc_t* callbacks, goldsrc::HUD_KB_FIND_FUNC find, std::string& error);
        void Shutdown();
        XashKeyQueries& Queries()
        {
            return queries_;
        }

    private:
        using Module = std::unique_ptr<void, int (*)(void*)>;
        Module engine_{nullptr, nullptr};
        XashKeyQueries queries_;
    };
} // namespace csr::client
