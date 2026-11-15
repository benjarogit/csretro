#pragma once

#include <goldsrc/client_api.h>
#include <array>
#include <cstdint>
#include <set>

namespace csr::client
{
    class VguiBinding;
    // Authenticated Xash key-event domain: 1..264, including international keys.
    // This differs from the old 256-entry GameUI key-query domain.
    // Press ownership survives menu/console visibility changes until key release.
    class NativeKeyCapture
    {
    public:
        using Capture = bool (*)(void*, int, int, const char*, bool repeat);
        int Event(goldsrc::HUD_KEY_EVENT_FUNC original, int down, int key, const char* binding, Capture capture, void* owner);
        int EventGuarded(
            VguiBinding& lifetime,
            goldsrc::HUD_KEY_EVENT_FUNC original,
            int down,
            int key,
            const char* binding,
            Capture capture,
            void* owner
        );

    private:
        enum class Owner : uint8_t
        {
            None,
            Game,
            Ui
        };
        std::array<Owner, 265> owners_{};
        std::array<uint64_t, 265> releases_{};
        std::set<int> releasing_;
        std::set<int> original_down_;
        bool active_{};
    };
} // namespace csr::client
