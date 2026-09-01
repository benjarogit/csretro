#pragma once

#include <cstdlib>

// Replaces SteamApps()->GetCurrentGameLanguage() in Scheme / Localize.
inline const char *Csretro_GetUiLanguage()
{
	const char *e = getenv("CSRETRO_UI_LANGUAGE");
	return e && *e ? e : "english";
}
