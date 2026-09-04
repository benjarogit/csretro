#include "menu_priv.h"

#include <cstring>
#include <cstdlib>

static const char *SkipWs(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;
	return s;
}

static std::string Unquote(const char *s, const char **end)
{
	s = SkipWs(s);
	std::string out;
	if (*s == '"')
	{
		s++;
		while (*s && *s != '"')
		{
			if (*s == '\\' && s[1])
				s++;
			out.push_back(*s++);
		}
		if (*s == '"')
			s++;
	}
	else
	{
		while (*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '{' && *s != '}')
			out.push_back(*s++);
	}
	if (end)
		*end = s;
	return out;
}

std::vector<ResField> Menu_LoadRes(const char *path)
{
	std::vector<ResField> fields;
	int len = 0;
	byte *raw = gEng.COM_LoadFile(path, &len);
	if (!raw || len <= 0)
		return fields;

	std::string text(reinterpret_cast<char *>(raw), static_cast<size_t>(len));
	gEng.COM_FreeFile(raw);

	ResField cur;
	bool have = false;
	int depth = 0;
	const char *p = text.c_str();
	while (*p)
	{
		p = SkipWs(p);
		if (!*p)
			break;
		if (*p == '/' && p[1] == '/')
		{
			while (*p && *p != '\n')
				p++;
			continue;
		}
		if (*p == '{')
		{
			depth++;
			p++;
			continue;
		}
		if (*p == '}')
		{
			if (have && depth == 2)
				fields.push_back(cur);
			have = false;
			cur = ResField();
			depth--;
			p++;
			continue;
		}

		const char *next = nullptr;
		std::string key = Unquote(p, &next);
		p = next;
		if (key.empty())
			break;

		p = SkipWs(p);
		if (*p == '{')
		{
			if (depth == 1)
			{
				cur = ResField();
				cur.name = key;
				have = true;
			}
			continue;
		}

		std::string val = Unquote(p, &next);
		p = next;
		if (!have)
			continue;
		if (!strcasecmp(key.c_str(), "fieldName"))
			cur.name = val;
		else if (!strcasecmp(key.c_str(), "ControlName"))
			cur.control = val;
		else if (!strcasecmp(key.c_str(), "labelText"))
			cur.label = val;
		else if (!strcasecmp(key.c_str(), "command") || !strcasecmp(key.c_str(), "Command"))
			cur.command = val;
		else if (!strcasecmp(key.c_str(), "xpos"))
			cur.x = atoi(val.c_str());
		else if (!strcasecmp(key.c_str(), "ypos"))
			cur.y = atoi(val.c_str());
		else if (!strcasecmp(key.c_str(), "wide"))
			cur.w = atoi(val.c_str());
		else if (!strcasecmp(key.c_str(), "tall"))
			cur.h = atoi(val.c_str());
		else if (!strcasecmp(key.c_str(), "visible"))
			cur.visible = atoi(val.c_str()) != 0;
		else if (!strcasecmp(key.c_str(), "enabled"))
			cur.enabled = atoi(val.c_str()) != 0;
	}
	return fields;
}

std::vector<GameMenuItem> Menu_LoadGameMenu()
{
	std::vector<GameMenuItem> items;
	int len = 0;
	byte *raw = gEng.COM_LoadFile("resource/GameMenu.res", &len);
	if (!raw || len <= 0)
		return items;
	std::string text(reinterpret_cast<char *>(raw), static_cast<size_t>(len));
	gEng.COM_FreeFile(raw);

	GameMenuItem cur;
	int depth = 0;
	bool have = false;
	const char *p = text.c_str();
	while (*p)
	{
		p = SkipWs(p);
		if (!*p)
			break;
		if (*p == '/' && p[1] == '/')
		{
			while (*p && *p != '\n')
				p++;
			continue;
		}
		if (*p == '{')
		{
			depth++;
			p++;
			continue;
		}
		if (*p == '}')
		{
			if (have && depth == 2)
				items.push_back(cur);
			have = false;
			cur = GameMenuItem();
			depth--;
			p++;
			continue;
		}
		const char *next = nullptr;
		std::string key = Unquote(p, &next);
		p = next;
		p = SkipWs(p);
		if (*p == '{')
		{
			if (depth == 1)
			{
				cur = GameMenuItem();
				have = true;
			}
			continue;
		}
		std::string val = Unquote(p, &next);
		p = next;
		if (!have)
			continue;
		if (key == "label")
		{
			cur.label = val;
			cur.empty = val.empty();
		}
		else if (key == "command")
			cur.command = val;
		else if (!strcasecmp(key.c_str(), "OnlyInGame"))
			cur.onlyInGame = atoi(val.c_str()) != 0;
		else if (!strcasecmp(key.c_str(), "notsingle"))
			cur.notSingle = atoi(val.c_str()) != 0;
		else if (!strcasecmp(key.c_str(), "notmulti"))
			cur.notMulti = atoi(val.c_str()) != 0;
	}
	return items;
}
