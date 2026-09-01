#include "menu_priv.h"

#include <cctype>
#include <cstring>
#include <unordered_map>

static std::unordered_map<std::string, std::string> gLoc;

static void LoadLocFile(const char *path)
{
	int len = 0;
	byte *raw = gEng.COM_LoadFile(path, &len);
	if (!raw || len <= 0)
		return;
	const char *p = reinterpret_cast<char *>(raw);
	const char *end = p + len;
	while (p < end)
	{
		while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if (p < end && *p == '/')
		{
			while (p < end && *p != '\n')
				p++;
			continue;
		}
		if (p >= end || *p != '"')
		{
			while (p < end && *p != '\n')
				p++;
			continue;
		}
		p++;
		std::string key;
		while (p < end && *p != '"')
			key.push_back(*p++);
		if (p < end && *p == '"')
			p++;
		while (p < end && (*p == ' ' || *p == '\t'))
			p++;
		if (p >= end || *p != '"')
			continue;
		p++;
		std::string val;
		while (p < end && *p != '"')
		{
			if (*p == '\\' && p + 1 < end)
			{
				p++;
				if (*p == 'n')
					val.push_back('\n');
				else
					val.push_back(*p);
				p++;
				continue;
			}
			val.push_back(*p++);
		}
		if (!key.empty())
			gLoc[key] = val;
		if (p < end && *p == '"')
			p++;
	}
	gEng.COM_FreeFile(raw);
}

void Menu_LoadLocale()
{
	gLoc.clear();
	LoadLocFile("resource/gameui_english.txt");
	LoadLocFile("resource/cstrike_english.txt");
	LoadLocFile("resource/vgui_english.txt");
	LoadLocFile("platform/resource/vgui_english.txt");
	LoadLocFile("platform/resource/platform_english.txt");
}

const char *Menu_L(const char *token)
{
	if (!token || !token[0])
		return "";
	if (token[0] == '#')
		token++;
	auto it = gLoc.find(token);
	if (it != gLoc.end())
		return it->second.c_str();
	return token;
}
