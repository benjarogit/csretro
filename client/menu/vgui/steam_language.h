#pragma once

#include <cstdlib>
#include <cstring>

inline const char *&Csretro_StoredUiLanguage()
{
	static const char *language = "english";
	return language;
}

inline bool Csretro_SetUiLanguage(const char *language)
{
	if (language && strcasecmp(language, "german") == 0)
	{
		Csretro_StoredUiLanguage() = "german";
		return true;
	}
	if (language && strcasecmp(language, "english") == 0)
	{
		Csretro_StoredUiLanguage() = "english";
		return true;
	}
	return false;
}

// Replaces SteamApps()->GetCurrentGameLanguage() in Scheme / Localize.
inline const char *Csretro_GetUiLanguage()
{
	const char *e = getenv("CSRETRO_UI_LANGUAGE");
	if (e && *e && Csretro_SetUiLanguage(e))
		return Csretro_StoredUiLanguage();
	return Csretro_StoredUiLanguage();
}
