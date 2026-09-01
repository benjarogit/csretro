#include "MenuEngine.h"

#include "../../src/menu_priv.h"

#include <cstring>

namespace MenuEngine
{
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
	gEng.pfnCvarSetValue(name, value);
}

void CvarSet(const char *name, const char *value)
{
	if (!name || !*name || !value || !gEng.pfnCvarSetString)
		return;
	gEng.pfnCvarSetString(name, value);
}

void ClientCmd(const char *cmd)
{
	if (!cmd || !*cmd || !gEng.pfnClientCmd)
		return;
	gEng.pfnClientCmd(0, cmd);
}

bool IsKeyDown(const char *keyName, bool &isDown)
{
	isDown = false;
	if (!keyName || !*keyName || !gEng.pfnKeyGetState)
		return false;
	// kbutton_t: state low bit = down (GoldSrc / Xash client).
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
} // namespace MenuEngine
