#pragma once

#include <memory>
#include <string>

namespace csr::client
{
    class NativeSdlInput
    {
    public:
        struct Hooks
        {
            bool (*needs_keyboard)(void*){};
            void (*mouse_move)(void*, int, int){};
            void (*mouse_button)(void*, int, int){};
            void (*reset_mouse)(void*){};
            void* context{};
        };

        NativeSdlInput();
        ~NativeSdlInput();
        NativeSdlInput(const NativeSdlInput&) = delete;
        NativeSdlInput& operator=(const NativeSdlInput&) = delete;
        bool Bind(void* client, void* vgui, const Hooks& hooks, std::string& error);
        void Drain();
        void HandleEvent(const void* event);
        void Clear();
        bool Shutdown();
        void EndNativeLifetime() noexcept;
        bool IsReady() const;
        bool ReleaseRequested() const;
        // The owner and borrowed modules remain live until this becomes false.
        bool HasPendingWatch() const;

    private:
        struct State;
        std::shared_ptr<State> state_;
    };
} // namespace csr::client
