#include "CreateGameSettingsList.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/TextEntry.h>

#include "../src/menu_priv.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern vgui2::ILocalize *g_pVGuiLocalize;

using namespace vgui2;

namespace
{
constexpr int kRowTall = 28;
constexpr int kControlTall = 24;
constexpr int kRowInset = 2;
constexpr int kNumberWide = 72;
constexpr int kListWide = 180;
constexpr int kCheckWide = 24;
constexpr int kListPad = 10;
constexpr int kControlGap = 8;

// settings.scr schreibt Zahlen als "20.000000". So stehenzulassen sähe im
// Eingabefeld falsch aus, deshalb Nachkommanullen kappen.
std::string TrimNumber(const std::string &value)
{
	if (value.find('.') == std::string::npos)
		return value;
	std::string out = value;
	while (out.size() > 1 && out.back() == '0')
		out.pop_back();
	if (!out.empty() && out.back() == '.')
		out.pop_back();
	return out.empty() ? std::string("0") : out;
}

// Grenzen aus settings.scr. -1 heißt dort „keine Grenze“, wie im Original.
std::string ClampNumber(const csretro::ScrOption &opt, const std::string &value)
{
	float v = static_cast<float>(atof(value.c_str()));
	if (opt.minValue != -1.f && v < opt.minValue)
		v = opt.minValue;
	if (opt.maxValue != -1.f && v > opt.maxValue)
		v = opt.maxValue;

	char buf[64];
	snprintf(buf, sizeof(buf), "%.6f", v);
	return TrimNumber(buf);
}

// hostname, maxplayers und sv_password stehen in settings.scr, gehören aber zur
// Serveridentität und liegen als getippte Felder im ServerProfile — Profile_Start
// braucht maxplayers als Zahl. Alle übrigen Einträge landen generisch in
// ServerProfile::gameplay. Fehlt ein Wert im Profil, gilt der Vorgabewert aus
// settings.scr.
std::string ValueFromProfile(const csretro::ScrOption &opt)
{
	char buf[64];
	if (opt.cvar == "hostname")
		return gProfile.hostname;
	if (opt.cvar == "sv_password")
		return gProfile.password;
	if (opt.cvar == "maxplayers")
	{
		snprintf(buf, sizeof(buf), "%d", gProfile.maxplayers);
		return buf;
	}
	for (const auto &kv : gProfile.gameplay)
		if (kv.first == opt.cvar)
			return kv.second;
	return TrimNumber(opt.defaultValue);
}

void ValueToProfile(const csretro::ScrOption &opt, const std::string &value)
{
	if (opt.cvar == "hostname")
	{
		gProfile.hostname = value;
		return;
	}
	if (opt.cvar == "sv_password")
	{
		gProfile.password = value;
		return;
	}
	if (opt.cvar == "maxplayers")
	{
		gProfile.maxplayers = atoi(value.c_str());
		return;
	}
	for (auto &kv : gProfile.gameplay)
	{
		if (kv.first == opt.cvar)
		{
			kv.second = value;
			return;
		}
	}
	gProfile.gameplay.emplace_back(opt.cvar, value);
}

// Innenliste ohne eigene Fläche — die äußere Liste bleibt das eine flache Rechteck.
class InnerSettingsList : public PanelListPanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(InnerSettingsList, PanelListPanel);

public:
	InnerSettingsList(Panel *parent, const char *name) : PanelListPanel(parent, name)
	{
		SetPaintBackgroundEnabled(false);
		SetPaintBorderEnabled(false);
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		PanelListPanel::ApplySchemeSettings(pScheme);
		SetPaintBackgroundEnabled(false);
		SetPaintBorderEnabled(false);
		SetBorder(nullptr);
	}
};
} // namespace

// Eine Zeile: Beschriftung links, Control rechts. Maße aus NextClient
// ScriptObject / CreateMultiplayerGameGameplayPage (Zeile 28, Control 24, Inset 2).
class CCreateGameSettingsList::SettingsRow : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(SettingsRow, Panel);

public:
	SettingsRow(Panel *parent, const char *name) : Panel(parent, name)
	{
		SetPaintBackgroundEnabled(false);
		SetPaintBorderEnabled(false);
	}

	void PerformLayout() override
	{
		Panel::PerformLayout();

		int wide = 0, tall = 0;
		GetSize(wide, tall);
		if (wide <= 0)
			return;

		// Prompt-Spalte: Originalformel wide/2 + 20. Controls teilen eine linke Kante.
		int promptW = wide / 2 + 20;
		if (promptW < 64)
			promptW = wide / 2;
		if (promptW > wide - 28)
			promptW = std::max(0, wide - 28);

		if (prompt)
			prompt->SetBounds(0, kRowInset, promptW, kControlTall);

		Panel *ctrl = Control();
		if (!ctrl)
			return;

		int ctrlW = 0;
		switch (opt.type)
		{
		case csretro::ScrType::Number:
			ctrlW = kNumberWide;
			break;
		case csretro::ScrType::List:
			ctrlW = kListWide;
			break;
		case csretro::ScrType::Bool:
			ctrlW = kCheckWide;
			break;
		case csretro::ScrType::String:
		default:
			ctrlW = std::max(0, wide - promptW - kControlGap);
			break;
		}

		const int remain = std::max(0, wide - promptW - kControlGap);
		if (ctrlW > remain)
			ctrlW = remain;
		ctrl->SetBounds(promptW, kRowInset, ctrlW, kControlTall);
	}

	Panel *Control() const
	{
		if (check)
			return check;
		if (combo)
			return combo;
		return entry;
	}

	csretro::ScrOption opt;
	Label *prompt = nullptr;
	CheckButton *check = nullptr;
	TextEntry *entry = nullptr;
	ComboBox *combo = nullptr;
};

CCreateGameSettingsList::CCreateGameSettingsList(Panel *parent, const char *name, csretro::ScrGroup group)
	: BaseClass(parent, name)
	, m_group(group)
{
	SetPaintBackgroundEnabled(true);
	SetPaintBorderEnabled(true);

	m_pList = new InnerSettingsList(this, "SettingsRows");
	m_pList->SetFirstColumnWidth(0);
	m_pList->SetVerticalBufferPixels(0);
	BuildRows();
}

void CCreateGameSettingsList::BuildRows()
{
	std::vector<csretro::ScrOption> options;
	if (!csretro::LoadServerSettingsScript("settings.scr", options))
		return;

	m_rows.reserve(options.size());
	for (const auto &opt : options)
	{
		if (csretro::GroupOfCvar(opt.cvar) != m_group)
			continue;

		SettingsRow *row = new SettingsRow(m_pList, "SettingsRow");
		row->opt = opt;
		row->SetSize(100, kRowTall);

		row->prompt = new Label(row, "OptionPrompt", opt.prompt.c_str());
		row->prompt->SetContentAlignment(Label::a_west);
		row->prompt->SetTextInset(8, 0);
		row->prompt->SetPaintBackgroundEnabled(false);

		switch (opt.type)
		{
		case csretro::ScrType::Bool:
			// Prompt bleibt eigenes Label — die Checkbox selbst bleibt klein.
			row->check = new CheckButton(row, "OptionCheck", "");
			break;

		case csretro::ScrType::List:
		{
			row->combo = new ComboBox(row, "OptionCombo", static_cast<int>(opt.items.size()), false);
			for (const auto &item : opt.items)
			{
				KeyValues *data = new KeyValues("data");
				data->SetString("value", item.value.c_str());

				// ComboBox übersetzt '#'-Token nicht selbst, anders als Label.
				wchar_t *text =
					g_pVGuiLocalize ? g_pVGuiLocalize->Find(item.text.c_str()) : nullptr;
				if (text && text[0])
					row->combo->AddItem(text, data);
				else
					row->combo->AddItem(item.text.c_str(), data);

				data->deleteThis();
			}
			break;
		}

		case csretro::ScrType::Number:
		case csretro::ScrType::String:
		default:
			row->entry = new TextEntry(row, "OptionEntry");
			break;
		}

		m_pList->AddItem(nullptr, row);
		m_rows.push_back(row);
	}
}

void CCreateGameSettingsList::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide = 0, tall = 0;
	GetSize(wide, tall);
	if (m_pList)
		m_pList->SetBounds(kListPad, kListPad, std::max(0, wide - 2 * kListPad),
			std::max(0, tall - 2 * kListPad));
}

void CCreateGameSettingsList::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	Color base = GetSchemeColor("ListPanel.BgColor", GetSchemeColor("WindowBG", pScheme), pScheme);
	if (base.a() == 0)
		base = GetSchemeColor("ControlBG", Color(40, 40, 40, 255), pScheme);
	SetBgColor(base);
	SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
	SetPaintBackgroundEnabled(true);
	SetPaintBorderEnabled(true);
}

std::string CCreateGameSettingsList::ReadRow(const SettingsRow &row) const
{
	if (row.check)
		return row.check->IsSelected() ? "1" : "0";

	if (row.combo)
	{
		if (KeyValues *data = row.combo->GetActiveItemUserData())
			return data->GetString("value", "");
		return row.opt.items.empty() ? std::string() : row.opt.items[0].value;
	}

	if (row.entry)
	{
		char buf[256];
		row.entry->GetText(buf, sizeof(buf));
		if (row.opt.type == csretro::ScrType::Number)
			return ClampNumber(row.opt, buf);
		return buf;
	}
	return std::string();
}

void CCreateGameSettingsList::WriteRow(SettingsRow &row, const std::string &value)
{
	if (row.check)
	{
		row.check->SilentSetSelected(atoi(value.c_str()) != 0);
		return;
	}

	if (row.combo)
	{
		for (size_t i = 0; i < row.opt.items.size(); ++i)
		{
			if (row.opt.items[i].value == value)
			{
				row.combo->SilentActivateItemByRow(static_cast<int>(i));
				return;
			}
		}
		if (!row.opt.items.empty())
			row.combo->SilentActivateItemByRow(0);
		return;
	}

	if (row.entry)
		row.entry->SetText(value.c_str());
}

void CCreateGameSettingsList::OnResetData()
{
	for (SettingsRow *row : m_rows)
	{
		if (row)
			WriteRow(*row, ValueFromProfile(row->opt));
	}
}

void CCreateGameSettingsList::OnApplyChanges()
{
	for (const SettingsRow *row : m_rows)
	{
		if (row)
			ValueToProfile(row->opt, ReadRow(*row));
	}
}

bool CCreateGameSettingsList::Gate_SetValue(const char *cvar, const char *value)
{
	if (!cvar)
		return false;
	for (SettingsRow *row : m_rows)
	{
		if (!row || row->opt.cvar != cvar)
			continue;
		WriteRow(*row, value ? value : "");
		return true;
	}
	return false;
}

bool CCreateGameSettingsList::Gate_AuditLabels()
{
	// Versteckte Tabs haben oft Breite 0 — dann wäre jedes Label-Audit sinnlos.
	int wide = 0, tall = 0;
	GetSize(wide, tall);
	if (wide < 200)
		SetSize(440, std::max(tall, kRowTall * std::max(1, Gate_RowCount()) + 2 * kListPad + 8));

	InvalidateLayout(true);
	if (m_pList)
		m_pList->InvalidateLayout(true);

	int pass = 0;
	for (SettingsRow *row : m_rows)
	{
		if (!row)
			continue;
		if (row->GetWide() < 200)
			row->SetSize(400, kRowTall);
		row->InvalidateLayout(true);

		Label *prompt = row->prompt;
		Panel *ctrl = row->Control();
		if (!prompt || !ctrl)
			continue;

		const int pw = prompt->GetWide();
		if (pw <= 0)
			continue;

		char text[256];
		prompt->GetText(text, sizeof(text));
		if (!text[0] || text[0] == '#')
			continue;

		int lx = 0, ly = 0, lw = 0, lh = 0;
		int cx = 0, cy = 0, cw = 0, ch = 0;
		prompt->GetBounds(lx, ly, lw, lh);
		ctrl->GetBounds(cx, cy, cw, ch);
		// Gemeinsame Kante (lx+lw == cx) ist kein Überlappen.
		if (lx + lw > cx && cx + cw > lx && ly + lh > cy && cy + ch > ly)
			continue;

		++pass;
	}

	Menu_Con("CSRETRO_CREATE_LABELS rows=%d pass=%d", Gate_RowCount(), pass);
	return pass == Gate_RowCount() && Gate_RowCount() > 0;
}
