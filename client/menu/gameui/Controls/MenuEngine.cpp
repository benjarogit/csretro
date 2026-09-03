#include "MenuEngine.h"

#include "../../src/menu_priv.h"
#include "../../vgui/xash_key_contract.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <strings.h>

namespace MenuEngine
{
namespace
{
void EnsureCvar(const char *name, const char *defValue)
{
	if (!name || !*name || !gEng.pfnRegisterVariable)
		return;
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

int KeyCount()
{
	return XashKey::Count();
}

bool KeyIsValid(int keynum)
{
	return XashKey::IsValidKeynum(keynum);
}

const char *KeynumToString(int keynum)
{
	if (!KeyIsValid(keynum) || !gEng.pfnKeynumToString)
		return "";
	const char *n = gEng.pfnKeynumToString(keynum);
	return n ? n : "";
}

const char *GetBinding(int keynum)
{
	if (!KeyIsValid(keynum) || !gEng.pfnKeyGetBinding)
		return "";
	const char *b = gEng.pfnKeyGetBinding(keynum);
	return b ? b : "";
}

void SetBinding(int keynum, const char *binding)
{
	if (!KeyIsValid(keynum) || !gEng.pfnKeySetBinding)
		return;
	if (XashKey::IsReserved(keynum))
	{
		const char *req = XashKey::ReservedBinding(keynum);
		if (!binding || !binding[0] || (req && req[0] && std::strcmp(binding, req) != 0))
		{
			gEng.pfnKeySetBinding(keynum, req && req[0] ? req : "cancelselect");
			return;
		}
	}
	gEng.pfnKeySetBinding(keynum, binding ? binding : "");
}

int KeyNameToKeynum(const char *name)
{
	if (!name || !*name)
		return -1;

	// Bounded scan over the central key space — no local convert table.
	// Letters: raw Xash path is lowercase ('c'==99). Prefer that over 'C'==67.
	int exact = -1;
	int ci = -1;
	int letterLower = -1;
	for (int i = 0; i < KeyCount(); ++i)
	{
		const char *n = KeynumToString(i);
		if (!n || !n[0])
			continue;
		if (exact < 0 && std::strcmp(n, name) == 0)
			exact = i;
		if (strcasecmp(n, name) != 0)
			continue;
		if (ci < 0)
			ci = i;
		if (n[1] == '\0')
		{
			const unsigned char c = static_cast<unsigned char>(n[0]);
			if (c >= 'a' && c <= 'z')
				letterLower = i;
		}
	}

	if (name[1] == '\0')
	{
		const unsigned char c = static_cast<unsigned char>(name[0]);
		if (std::isalpha(c) && letterLower >= 0)
			return letterLower;
	}
	if (exact >= 0)
		return exact;
	if (ci >= 0)
		return ci;
	return -1;
}

bool HasExtendedEngfuncs()
{
	return gExtEngReady && gExtEng.pfnEnableTextInput != nullptr;
}

void EnableTextInput(bool enable)
{
	if (!HasExtendedEngfuncs())
		return;
	gExtEng.pfnEnableTextInput(enable ? 1 : 0);
}
} // namespace MenuEngine
