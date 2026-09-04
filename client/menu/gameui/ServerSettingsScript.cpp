#include "ServerSettingsScript.h"

#include <FileSystem.h>
#include <tier1/utlbuffer.h>
#include <vgui_controls/Controls.h>

#include "../src/menu_priv.h"

#include <cstdlib>
#include <cstring>

namespace csretro
{
namespace
{

// Minimaler Tokenizer für das Script-Format: Kommentare (//) fallen weg,
// geschweifte Klammern sind eigene Token, Anführungszeichen klammern Text.
class Tokenizer
{
public:
	explicit Tokenizer(const char *text) : m_p(text) {}

	// Liefert false am Ende. `quoted` unterscheidet "Text" von blankem WORT.
	bool Next(std::string &token, bool &quoted)
	{
		SkipBlanks();
		if (!m_p || !*m_p)
			return false;

		quoted = false;
		token.clear();

		if (*m_p == '"')
		{
			quoted = true;
			++m_p;
			while (*m_p && *m_p != '"')
				token.push_back(*m_p++);
			if (*m_p == '"')
				++m_p;
			return true;
		}

		if (*m_p == '{' || *m_p == '}')
		{
			token.push_back(*m_p++);
			return true;
		}

		while (*m_p && !isspace(static_cast<unsigned char>(*m_p)) && *m_p != '{' && *m_p != '}' &&
			*m_p != '"')
			token.push_back(*m_p++);
		return !token.empty();
	}

private:
	void SkipBlanks()
	{
		for (;;)
		{
			while (*m_p && isspace(static_cast<unsigned char>(*m_p)))
				++m_p;
			if (m_p[0] == '/' && m_p[1] == '/')
			{
				while (*m_p && *m_p != '\n')
					++m_p;
				continue;
			}
			return;
		}
	}

	const char *m_p;
};

ScrType TypeFromWord(const std::string &word)
{
	if (word == "BOOL")
		return ScrType::Bool;
	if (word == "NUMBER")
		return ScrType::Number;
	if (word == "LIST")
		return ScrType::List;
	return ScrType::String;
}

// Liest den Typblock: { TYPE [Typinfo] }. Erwartet, dass '{' schon konsumiert ist.
bool ParseTypeBlock(Tokenizer &tk, ScrOption &opt)
{
	std::string tok;
	bool quoted = false;

	if (!tk.Next(tok, quoted) || quoted)
		return false;
	opt.type = TypeFromWord(tok);

	switch (opt.type)
	{
	case ScrType::Number:
	{
		std::string lo, hi;
		if (!tk.Next(lo, quoted) || !tk.Next(hi, quoted))
			return false;
		opt.minValue = static_cast<float>(atof(lo.c_str()));
		opt.maxValue = static_cast<float>(atof(hi.c_str()));
		break;
	}
	case ScrType::List:
	{
		// Paare "Anzeigetext" "Wert" bis zur schließenden Klammer.
		for (;;)
		{
			if (!tk.Next(tok, quoted))
				return false;
			if (!quoted && tok == "}")
				return true;
			ScrListItem item;
			item.text = tok;
			if (!tk.Next(item.value, quoted))
				return false;
			opt.items.push_back(item);
		}
	}
	default:
		break;
	}

	// Bool/String/Number: schließende Klammer des Typblocks.
	if (!tk.Next(tok, quoted) || quoted || tok != "}")
		return false;
	return true;
}

bool ParseOption(Tokenizer &tk, ScrOption &opt)
{
	std::string tok;
	bool quoted = false;

	// { "#Prompt" { TYPE … } { "default" } }
	if (!tk.Next(tok, quoted) || tok != "{")
		return false;
	if (!tk.Next(opt.prompt, quoted) || !quoted)
		return false;
	if (!tk.Next(tok, quoted) || tok != "{")
		return false;
	if (!ParseTypeBlock(tk, opt))
		return false;
	if (!tk.Next(tok, quoted) || tok != "{")
		return false;
	if (!tk.Next(opt.defaultValue, quoted) || !quoted)
		return false;
	if (!tk.Next(tok, quoted) || tok != "}")
		return false;
	if (!tk.Next(tok, quoted) || tok != "}")
		return false;
	return true;
}

bool ReadWholeFile(const char *fileName, CUtlBuffer &buf, int &size)
{
	if (!::g_pFullFileSystem || !fileName)
		return false;
	FileHandle_t fh = ::g_pFullFileSystem->Open(fileName, "rb", "GAME");
	if (fh == FILESYSTEM_INVALID_HANDLE)
		fh = ::g_pFullFileSystem->Open(fileName, "rb");
	if (fh == FILESYSTEM_INVALID_HANDLE)
		return false;

	size = ::g_pFullFileSystem->Size(fh);
	buf.EnsureCapacity(size + 1);
	::g_pFullFileSystem->Read(buf.Base(), size, fh);
	::g_pFullFileSystem->Close(fh);
	static_cast<char *>(buf.Base())[size] = '\0';
	return size > 0;
}

} // namespace

bool LoadServerSettingsScript(const char *fileName, std::vector<ScrOption> &out)
{
	out.clear();

	CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);
	int size = 0;
	if (!ReadWholeFile(fileName, buf, size))
	{
		Menu_Con("CSRETRO_SCR missing %s", fileName ? fileName : "?");
		return false;
	}

	Tokenizer tk(static_cast<const char *>(buf.Base()));
	std::string tok;
	bool quoted = false;

	// Kopf überspringen: VERSION <float> DESCRIPTION <name> {
	bool inBlock = false;
	while (tk.Next(tok, quoted))
	{
		if (!quoted && tok == "{")
		{
			inBlock = true;
			break;
		}
	}
	if (!inBlock)
	{
		Menu_Con("CSRETRO_SCR malformed %s (kein Block)", fileName);
		return false;
	}

	while (tk.Next(tok, quoted))
	{
		if (!quoted && tok == "}")
			break;
		if (!quoted)
			continue; // unerwartetes Wort — überspringen statt abbrechen

		ScrOption opt;
		opt.cvar = tok;
		if (!ParseOption(tk, opt))
		{
			Menu_Con("CSRETRO_SCR malformed %s bei \"%s\"", fileName, opt.cvar.c_str());
			return false;
		}
		out.push_back(opt);
	}

	Menu_Con("CSRETRO_SCR %s entries=%d", fileName, static_cast<int>(out.size()));
	return !out.empty();
}

ScrGroup GroupOfCvar(const std::string &cvar)
{
	// Genau die drei Einträge, die Profile_Start als getippte Felder braucht.
	if (cvar == "hostname" || cvar == "maxplayers" || cvar == "sv_password")
		return ScrGroup::Identity;

	static const char *const kFairness[] = {"mp_friendlyfire", "mp_tkpunish", "mp_autokick",
		"mp_autoteambalance", "mp_limitteams", "mp_hostagepenalty", "mp_forcecamera",
		"mp_fadetoblack", "mp_playerid", "allow_spectators"};
	for (const char *name : kFairness)
	{
		if (cvar == name)
			return ScrGroup::Fairness;
	}

	return ScrGroup::Rules;
}

} // namespace csretro
