#include "NativeKeyCapture.h"
#include "vgui_binding.h"

namespace csr::client
{
    int NativeKeyCapture::EventGuarded(
        VguiBinding& lifetime,
        goldsrc::HUD_KEY_EVENT_FUNC original,
        int down,
        int key,
        const char* binding,
        Capture capture,
        void* owner
    )
    {
        struct Request
        {
            NativeKeyCapture* keys;
            VguiBinding* lifetime;
            goldsrc::HUD_KEY_EVENT_FUNC original;
            int down, key;
            const char* binding;
            Capture capture;
            void* owner;
            bool entered{};
            int result{};
        } request{this, &lifetime, original, down, key, binding, capture, owner};
        lifetime.RunKeyInput(
            [](void* opaque) {
                auto& request = *static_cast<Request*>(opaque);
                request.entered = true;
                const Capture policy =
                    request.lifetime->CanDispatchNativeUi() ? request.capture : +[](void*, int, int, const char*, bool) { return true; };
                request.result = request.keys->Event(request.original, request.down, request.key, request.binding, policy, request.owner);
            },
            &request
        );
        // Teardown may finish after executing the operation, making RunKeyInput
        // return false. The original must never be dispatched a second time.
        if (request.entered)
        {
            return request.result;
        }
        const Capture fallback = lifetime.HasBinding() ? +[](void*, int, int, const char*, bool) { return true; } : nullptr;
        return Event(original, down, key, binding, fallback, nullptr);
    }

    int NativeKeyCapture::Event(goldsrc::HUD_KEY_EVENT_FUNC original, int down, int key, const char* binding, Capture capture, void* owner)
    {
        const bool valid = key > 0 && key < static_cast<int>(owners_.size());
        struct ActiveCall
        {
            bool& active;
            bool previous;
            explicit ActiveCall(bool& value) :
                active(value),
                previous(value)
            {
                active = true;
            }
            ~ActiveCall()
            {
                active = previous;
            }
        };
        if (!down)
        {
            // Focus/menu callbacks can release gameplay keys from inside capture.
            // Only recursive release of this same key is suppressed.
            if (!releasing_.insert(key).second)
            {
                return 0;
            }
            struct ReleaseCall
            {
                std::set<int>& releasing;
                int key;
                ~ReleaseCall()
                {
                    releasing.erase(key);
                }
            } release{releasing_, key};
            ActiveCall call(active_);
            if (valid)
            {
                ++releases_[key];
                const auto previous = owners_[key];
                owners_[key] = Owner::None;
                if (previous == Owner::Ui)
                {
                    return 0;
                }
            }
            return original ? original(down, key, binding) : 0;
        }
        auto forward_down = [&] {
            if (!original_down_.insert(key).second)
            {
                return 0;
            }
            struct OriginalDownCall
            {
                std::set<int>& keys;
                int key;
                ~OriginalDownCall()
                {
                    keys.erase(key);
                }
            } original_call{original_down_, key};
            ActiveCall call(active_);
            return original ? original(down, key, binding) : 0;
        };
        // Gameplay owns the whole press stream, including repeats issued from
        // another key's capture. Only same-key original recursion is blocked.
        // Invalid keys are outside capture's domain and use that same safeguard.
        if (!valid || owners_[key] == Owner::Game)
        {
            return forward_down();
        }
        if (active_)
        {
            // A nested fresh press cannot acquire gameplay without capture.
            if (owners_[key] == Owner::None)
            {
                owners_[key] = Owner::Ui;
            }
            return 0;
        }
        ActiveCall call(active_);
        auto& pressed = owners_[key];
        const bool repeat = pressed != Owner::None;
        const auto previous = pressed;
        if (!repeat)
        {
            // Capture may throw. Its unfinished fresh press stays out of gameplay.
            pressed = Owner::Ui;
        }
        const auto release_generation = releases_[key];
        const bool consumed = capture && capture(owner, down, key, binding, repeat);
        if (previous == Owner::Ui || consumed || releases_[key] != release_generation)
        {
            return 0;
        }
        pressed = Owner::Game;
        return forward_down();
    }
} // namespace csr::client
