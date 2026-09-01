#pragma once

// Thin Xash MenuAPI wrappers for NextClient GameUI controls.
// Replaces engine->pfnGetCvarFloat / Cvar_Set / g_pGameUIFuncs->IsKeyDown.
namespace MenuEngine
{
float GetCvarFloat(const char *name);
const char *GetCvarString(const char *name);
void CvarSetValue(const char *name, float value);
void CvarSet(const char *name, const char *value);
void ClientCmd(const char *cmd);
bool IsKeyDown(const char *keyName, bool &isDown);
}
