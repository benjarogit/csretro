#pragma once

#include <string>
#include <vector>

// Gezielter Port der Parse-Semantik von NextClient `ScriptObject`/`CDescription`
// (`docs/UPSTREAM.md`). Übernommen ist nur das Lesen von `settings.scr`; das Original
// erbt zusätzlich von vgui2::Panel und schreibt CVars und Configs selbst — beides
// brauchen wir nicht, weil das ServerProfile die einzige Konfigurationsquelle bleibt.
//
// Format (siehe Kopf von cstrike/settings.scr):
//   "cvar" { "#Prompt" { TYPE [Typinfo] } { "default" } }
//   TYPE = BOOL | NUMBER min max | STRING | LIST "Text" "Wert" …
//   Bei NUMBER heißt -1 „keine Grenze“.
namespace csretro
{

enum class ScrType
{
	Bool,
	Number,
	String,
	List,
};

struct ScrListItem
{
	std::string text;  // Localization-Token oder Klartext
	std::string value; // was in die CVar geht
};

struct ScrOption
{
	std::string cvar;
	std::string prompt;
	ScrType type = ScrType::String;
	std::string defaultValue;

	// Nur Number. -1 bedeutet unbegrenzt, wie im Original.
	float minValue = -1.f;
	float maxValue = -1.f;

	// Nur List.
	std::vector<ScrListItem> items;
};

// Liest die Datei über das Spiel-Filesystem (Suchpfade wie bei .res).
// Liefert false, wenn die Datei fehlt oder keine Einträge enthält.
bool LoadServerSettingsScript(const char *fileName, std::vector<ScrOption> &out);

// Auf welche Seite des Create-Game-Dialogs ein Eintrag gehört. settings.scr kennt
// keine Gruppen — 24 Zeilen in einer Liste sind aber unbedienbar, deshalb ordnet der
// Dialog sie zu. `Rules` ist der Auffangwert: eine neue Zeile in settings.scr
// erscheint damit ohne Codeänderung, statt zu verschwinden.
enum class ScrGroup
{
	Identity, // Servername, Slots, Passwort — Server-Seite neben der Map
	Rules,    // Runde, Zeit, Geld
	Fairness, // Team, Bestrafung, Zuschauer
};

ScrGroup GroupOfCvar(const std::string &cvar);

} // namespace csretro
