#pragma once

#include <string>
#include <vector>

// Thin Xash MenuAPI wrappers for NextClient GameUI controls.
namespace MenuEngine
{
float GetCvarFloat(const char *name);
const char *GetCvarString(const char *name);
void CvarSetValue(const char *name, float value);
void CvarSet(const char *name, const char *value);
void ClientCmd(const char *cmd);
void ClientCmdNow(const char *cmd);
void ConsolePrint(const char *text);
void SetKeyDest(int destination);
// Restore gameplay input only after the overlay click's mouse button is up.
// Grabbing the mouse while MOUSE1 is still down eats the next +attack (grenade pin).
void RestoreGameKeyDest();
void PollPendingGameKeyDest();
bool IsKeyDown(const char *keyName, bool &isDown);
const char *GetModeString(int modeIndex);

// Direct key binding API (canonical identity = Xash keynum). Bounds-checked.
int KeyCount();
bool KeyIsValid(int keynum);
const char *KeynumToString(int keynum);
const char *GetBinding(int keynum);
void SetBinding(int keynum, const char *binding);
int KeyNameToKeynum(const char *name);

// Extended MenuAPI — controls SDL text input (printables as Key_Event vs Char).
void EnableTextInput(bool enable);
bool HasExtendedEngfuncs();

// Console TAB-complete. Prefix of the first token; empty if the line already has a space.
void CollectConsoleCompletions(const char *prefix, std::vector<std::string> *names);
} // namespace MenuEngine
