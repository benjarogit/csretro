#pragma once

#include <string>
#include <cstdint>
#include <goldsrc/client_api.h>

namespace csr::client
{
    // The caller owns the module and must retain it until all callbacks finish.
    // This reads the original F table; it supplies no substitute callbacks.
    bool ReadCallbacks(void* module, goldsrc::cldll_func_t& out, std::string& error);

    using BindVgui = bool (*)(void* owner, goldsrc::cl_enginefunc_t* engine);
    int InitializeWithVgui(goldsrc::INITIALIZE_FUNC original, goldsrc::cl_enginefunc_t* engine,
                           int version, BindVgui bind, void* owner);
    using UiAction = void (*)(void* owner);
    using UiMouse = void (*)(void* owner, int buttons);
    int RedrawWithVgui(goldsrc::HUD_REDRAW_FUNC original, float time, int intermission, UiAction after, void* owner);
    void FrameWithVgui(goldsrc::HUD_FRAME_FUNC original, double time, UiAction after, void* owner);
    void MouseWithVgui(goldsrc::HUD_IN_MOUSEEVENT_FUNC original, int buttons, UiAction before, UiMouse after, void* owner);
    void ResetInputWithVgui(goldsrc::HUD_IN_CLEARSTATES_FUNC original, UiAction after, void* owner);

    class MouseButtonState
    {
    public:
        using Emit = void (*)(void* owner, int button, int down);
        void Update(int buttons, Emit emit, void* owner);
        void Clear(Emit emit, void* owner);
    private:
        static constexpr int button_count = 5;
        unsigned held_{};
        uint64_t revision_{};
    };
}
