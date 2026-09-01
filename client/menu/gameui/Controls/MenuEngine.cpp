#include "MenuEngine.h"

#include "../../src/menu_priv.h"

#include <cstdio>
#include <cstring>

namespace MenuEngine
{
namespace
{
void EnsureCvar(const char *name, const char *defValue)
{
	if (!name || !*name || !gEng.pfnRegisterVariable)
		return;
	// MenuAPI: get-or-create (needed before client.dll registers sensitivity etc.)
	gEng.pfnRegisterVariable(name, defValue ? defValue : "0", 0);
}
} // namespace

float GetCvarFloat(const char *name)
{
	if (!name || !*name || !gEng.pfnGetCvarFloat)
		return 0.f;
	return gEng.pfnGetCvarFloat(name);
}

const char *GetCvarString(const char *name)
{
	if (!name || !*name || !gEng.pfnGetCvarString)
		return "";
	const char *v = gEng.pfnGetCvarString(name);
	return v ? v : "";
}

void CvarSetValue(const char *name, float value)
{
	if (!name || !*name || !gEng.pfnCvarSetValue)
		return;
	EnsureCvar(name, "0");
	gEng.pfnCvarSetValue(name, value);
}

void CvarSet(const char *name, const char *value)
{
	if (!name || !*name || !value || !gEng.pfnCvarSetString)
		return;
	EnsureCvar(name, value);
	gEng.pfnCvarSetString(name, value);
}

void ClientCmd(const char *cmd)
{
	if (!cmd || !*cmd || !gEng.pfnClientCmd)
		return;
	gEng.pfnClientCmd(0, cmd);
}

void ClientCmdNow(const char *cmd)
{
	if (!cmd || !*cmd || !gEng.pfnClientCmd)
		return;
	gEng.pfnClientCmd(1, cmd);
}

bool IsKeyDown(const char *keyName, bool &isDown)
{
	isDown = false;
	if (!keyName || !*keyName || !gEng.pfnKeyGetState)
		return false;
	struct LocalKButton
	{
		int down[2];
		int state;
	};
	auto *btn = static_cast<LocalKButton *>(gEng.pfnKeyGetState(keyName));
	if (!btn)
		return false;
	isDown = (btn->state & 1) != 0;
	return true;
}

const char *GetModeString(int modeIndex)
{
	if (!gEng.pfnGetModeString)
		return nullptr;
	return gEng.pfnGetModeString(modeIndex);
}
} // namespace MenuEngine
