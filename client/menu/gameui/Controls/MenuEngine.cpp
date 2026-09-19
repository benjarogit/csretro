#include "MenuEngine.h"
#include "../GameConsoleDialog.h"

#include "../../src/menu_priv.h"
#include "../../vgui/xash_key_contract.h"
#include "keydefs.h"

#ifndef KEY_DEST_GAME
#define KEY_DEST_GAME 1
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <strings.h>
#include <vector>

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

void ConsolePrint(const char *text)
{
	if (text && *text && gEng.Con_Printf)
		gEng.Con_Printf("%s", text);
}

void SetKeyDest(int destination)
{
	if (gEng.pfnSetKeyDest)
		gEng.pfnSetKeyDest(destination);
}

namespace
{
bool s_pendingGameKeyDest = false;

bool Mouse1PhysicallyDown()
{
	return gEng.pfnKeyIsDown && gEng.pfnKeyIsDown(K_MOUSE1);
}
} // namespace

void RestoreGameKeyDest()
{
	if (GameConsole_IsActive())
		return;
	if (Mouse1PhysicallyDown())
	{
		s_pendingGameKeyDest = true;
		Menu_Con("CSRETRO_INPUT defer_game_dest mouse1_down");
		return;
	}
	s_pendingGameKeyDest = false;
	SetKeyDest(KEY_DEST_GAME);
}

void PollPendingGameKeyDest()
{
	if (!s_pendingGameKeyDest)
		return;
	if (Mouse1PhysicallyDown())
		return;
	s_pendingGameKeyDest = false;
	Menu_Con("CSRETRO_INPUT restore_game_dest");
	SetKeyDest(KEY_DEST_GAME);
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

void CollectConsoleCompletions(const char *prefix, std::vector<std::string> *names)
{
	if (!names)
		return;
	names->clear();
	if (!prefix || !*prefix)
		return;
	if (std::strchr(prefix, ' '))
		return;
	if (!gExtEngReady || !gExtEng.pfnGetFirstCmdFunctionHandle || !gExtEng.pfnGetNextCmdFunctionHandle ||
		!gExtEng.pfnGetCmdFunctionName)
		return;

	const size_t prefixLen = std::strlen(prefix);
	auto prefixMatch = [prefix, prefixLen](const char *name) -> bool {
		if (!name || !*name)
			return false;
		return strncasecmp(name, prefix, prefixLen) == 0;
	};

	for (void *cmd = gExtEng.pfnGetFirstCmdFunctionHandle(); cmd;
		 cmd = gExtEng.pfnGetNextCmdFunctionHandle(cmd))
	{
		const char *name = gExtEng.pfnGetCmdFunctionName(cmd);
		if (prefixMatch(name))
			names->emplace_back(name);
	}

	if (gExtEng.pfnGetFirstCvarPtr)
	{
		for (cvar_t *cv = gExtEng.pfnGetFirstCvarPtr(); cv; cv = cv->next)
		{
			if (prefixMatch(cv->name))
				names->emplace_back(cv->name);
		}
	}

	std::sort(names->begin(), names->end());
	names->erase(std::unique(names->begin(), names->end()), names->end());
}

bool SendWarmupReadyKey(int xashKey, bool down)
{
	if (!down || xashKey != K_F12)
		return false;
	ClientCmdNow("ready\n");
	return true;
}
} // namespace MenuEngine
