// Synthetic engine/client key services. No game or engine entry is executed.
#include <goldsrc/client_api.h>
#include <goldsrc/usercmd.h>
#include <array>
#include <cstring>
#include <string>
#include <strings.h>
namespace
{
    std::array<std::string, 265> bindings;
    char scratch[2];
    int invalid_queries{}, host_calls{}, find_calls{};
    goldsrc::kbutton_t action{};
    const char* Special(int key)
    {
        switch (key)
        {
            case 9: return "TAB";
            case 13: return "ENTER";
            case 27: return "ESCAPE";
            case 32: return "SPACE";
            case 127: return "BACKSPACE";
            case 128: return "UPARROW";
            case 129: return "DOWNARROW";
            case 130: return "LEFTARROW";
            case 131: return "RIGHTARROW";
            case 132: return "ALT";
            case 133: return "CTRL";
            case 134: return "SHIFT";
            case 135: return "F1";
            case 147: return "INS";
            case 148: return "DEL";
            case 149: return "PGDN";
            case 150: return "PGUP";
            case 160: return "KP_HOME";
            case 172: return "KP_SLASH";
            case 200: return "MOUSE1";
            default: return "<UNKNOWN KEYNUM>";
        }
    }
}
extern "C" __attribute__((visibility("hidden"))) const char* Key_KeynumToString(int key)
{
    if (key < 0 || key >= 265) { ++invalid_queries; return nullptr; }
    if (key > 32 && key < 127) { scratch[0] = static_cast<char>(key); scratch[1] = 0; return scratch; }
    return Special(key);
}
extern "C" __attribute__((visibility("hidden"))) const char* Key_GetBinding(int key)
{
    if (key < 0 || key >= 265) { ++invalid_queries; return nullptr; }
    return bindings[key].empty() ? nullptr : bindings[key].c_str();
}
namespace
{
    const char* Lookup(const char* command)
    {
        for (int key = 0; key < 265; ++key)
            if (!bindings[key].empty() && bindings[key] == command) return Key_KeynumToString(key);
        return nullptr;
    }
    goldsrc::cl_enginefunc_t callbacks{};
}
extern "C" goldsrc::kbutton_t* FixtureFindAction(const char* name)
{
    ++find_calls;
    return name && std::strcmp(name, "in_mlook") == 0 ? &action : nullptr;
}
extern "C" int SDL_GetKeyFromName(const char* name)
{
    struct Named { const char* name; int value; };
    static const Named names[] = {{"Up",1073741906},{"Down",1073741905},{"Left",1073741904},{"Right",1073741903},
        {"F1",1073741882},{"Left Alt",1073742050},{"Left Ctrl",1073742048},{"Left Shift",1073742049},
        {"Keypad 7",1073741919},{"Keypad /",1073741908},{"Insert",1073741897},{"Delete",127},
        {"PageUp",1073741899},{"PageDown",1073741902},{"SPACE",32},{"TAB",9},{"ENTER",13},{"ESCAPE",27}};
    if (!name) return 0;
    if (name[0] && !name[1]) return static_cast<unsigned char>(name[0]);
    for (const auto& entry : names) if (!strcasecmp(name, entry.name)) return entry.value;
    return 0;
}
extern "C" int Host_Main() { ++host_calls; return -1; }
extern "C" void FixtureKeysReset()
{
    for (auto& value : bindings) value.clear();
    bindings[97] = "+attack";
    callbacks = {};
    callbacks.Key_LookupBinding = Lookup;
    invalid_queries = host_calls = find_calls = 0;
    action = {{0,0},1};
}
extern "C" goldsrc::cl_enginefunc_t* FixtureKeyCallbacks() { return &callbacks; }
extern "C" void FixtureSetBinding(int key, const char* command)
{
    if (key >= 0 && key < 265) bindings[key] = command ? command : "";
}
extern "C" void FixtureSetActionState(int state) { action.state = state; }
extern "C" int FixtureInvalidKeyQueries() { return invalid_queries; }
extern "C" int FixtureHostMainCalls() { return host_calls; }
extern "C" int FixtureFindCalls() { return find_calls; }
