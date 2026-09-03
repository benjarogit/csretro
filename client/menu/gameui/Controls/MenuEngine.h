#pragma once

// Thin Xash MenuAPI wrappers for NextClient GameUI controls.
namespace MenuEngine
{
float GetCvarFloat(const char *name);
const char *GetCvarString(const char *name);
void CvarSetValue(const char *name, float value);
void CvarSet(const char *name, const char *value);
void ClientCmd(const char *cmd);
void ClientCmdNow(const char *cmd);
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
} // namespace MenuEngine
