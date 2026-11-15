// Independently compiled old GameUI ABI consumer; no producer/SDK headers.
#include "GameUiProbe.h"
#include <cstdio>
namespace legacy_gameui_abi
{
    struct Mode { int w, h, bpp; };
    class OriginalFuncs
    {
    public:
        virtual ~OriginalFuncs() = default;
        virtual bool IsKeyDown(const char*, bool&) = 0;
        virtual const char* Key_NameForKey(int) = 0;
        virtual const char* Key_BindingForKey(int) = 0;
        virtual int GetVGUI2KeyCodeForBind(const char*) = 0;
        virtual void GetVideoModes(Mode**, int*) = 0;
        virtual void GetCurrentVideoMode(int*, int*, int*) = 0;
        virtual void GetCurrentRenderer(char*, int, int*, int*, int*) = 0;
        virtual bool IsConnectedToVACSecureServer() = 0;
        virtual int Key_KeyStringToKeyNum(const char*) = 0;
    };
}
extern "C" void FixtureConsumeGameUi(void* object, const char* action, const char* command,
                                    int key, const char* name, GameUiProbe* result)
{
    auto* api = static_cast<legacy_gameui_abi::OriginalFuncs*>(object);
    *result = {};
    bool down{};
    result->exists = api->IsKeyDown(action, down);
    result->down = down;
    std::snprintf(result->key_name, sizeof(result->key_name), "%s", api->Key_NameForKey(key));
    std::snprintf(result->binding, sizeof(result->binding), "%s", api->Key_BindingForKey(key));
    result->vgui_code = api->GetVGUI2KeyCodeForBind(command);
    legacy_gameui_abi::Mode* modes{};
    api->GetVideoModes(&modes, &result->count);
    if (modes && result->count > 0)
    {
        result->mode_wide = modes[0].w;
        result->mode_tall = modes[0].h;
        result->mode_bpp = modes[0].bpp;
    }
    api->GetCurrentVideoMode(&result->current_wide, &result->current_tall, &result->current_bpp);
    result->guard = 0x12345678;
    api->GetCurrentRenderer(result->renderer, sizeof(result->renderer), &result->windowed, &result->hd, &result->addons);
    result->vac = api->IsConnectedToVACSecureServer();
    result->key_number = api->Key_KeyStringToKeyNum(name);
}
