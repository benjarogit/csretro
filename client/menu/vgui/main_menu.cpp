#include "main_menu.h"

#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <vgui/ISchemeNext.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/Panel.h>

#include <tier1/KeyValues.h>

#include <cstdlib>
#include <cstring>

#include "keydefs.h"

#include "../src/menu_priv.h"
#include "vgui_boot.h"

using namespace vgui2;

extern vgui2::IInputInternal *g_pVGuiInput;

// Gate-Treiber: echte Menü-Einstiegspunkte statt VGUI-Interna, damit die
// Regression denselben Weg nimmt wie eine Maus oder Tastatur am Fenster.
void UI_KeyEvent(int key, int down);
void UI_MouseMove(int x, int y);

namespace
{
// Classic look comes from TrackerScheme "InGameDesktop"; keep Valve's defaults as fallback
// so a stripped scheme still renders a readable menu instead of black on black.
const Color kFallbackMenuColor(238, 174, 0, 255);
const Color kFallbackArmedColor(255, 186, 0, 255);
const Color kFallbackDepressedColor(16, 16, 16, 255);
const int kFallbackItemHeight = 24;
const int kFallbackInset = 24;

int SchemeInt(IScheme *scheme, const char *key, int fallback)
{
	if (!scheme)
		return fallback;
	const char *value = scheme->GetResourceString(key);
	if (!value || !*value)
		return fallback;
	return atoi(value);
}

class CsretroGameMenuItem : public MenuItem
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CsretroGameMenuItem, MenuItem);

public:
	CsretroGameMenuItem(Menu *parent, const char *name, const char *text)
		: BaseClass(parent, name, text)
	{
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);

		const Color fg = GetSchemeColor("InGameDesktop/MenuColor", kFallbackMenuColor, scheme);
		const Color armed = GetSchemeColor("InGameDesktop/ArmedMenuColor", kFallbackArmedColor, scheme);
		const Color depressed = GetSchemeColor("InGameDesktop/DepressedMenuColor", kFallbackDepressedColor, scheme);
		const Color transparent(0, 0, 0, 0);

		SetFgColor(fg);
		SetBgColor(transparent);
		SetDefaultColor(fg, transparent);
		SetArmedColor(armed, transparent);
		SetDepressedColor(depressed, transparent);

		// The background image carries the frame; menu entries are plain text.
		SetPaintBackgroundEnabled(false);
		SetBorder(nullptr);
		SetDefaultBorder(nullptr);
		SetDepressedBorder(nullptr);
		SetKeyFocusBorder(nullptr);
		SetContentAlignment(Label::a_west);
		SetTextInset(0, 0);
		SetButtonActivationType(Button::ACTIVATE_ONPRESSED);

		if (scheme)
		{
			HFont font = scheme->GetFont("MenuLarge", IsProportional());
			if (font == INVALID_FONT)
				font = scheme->GetFont("Default", IsProportional());
			if (font != INVALID_FONT)
				SetFont(font);
		}
	}
};

// vgui2::Menu is built as a popup that hides itself on focus loss. The main menu is
// permanent furniture, so visibility is owned by MainMenu_Show/Hide, not by focus.
class CsretroGameMenu : public Menu
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CsretroGameMenu, Menu);

public:
	CsretroGameMenu(Panel *parent, const char *name) : BaseClass(parent, name)
	{
		// CMenuManager schließt bei jedem Klick außerhalb alle registrierten Menüs
		// (OnInternalMousePressed → AbortMenus). Das gilt für aufklappende Popup-Menüs;
		// das Hauptmenü ist dauerhaft und würde nach dem ersten Klick in einen Dialog
		// verschwinden.
		EnableUseMenuManager(false);
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);

		int itemHeight = SchemeInt(scheme, "InGameDesktop/MenuItemHeight", 0);
		if (itemHeight <= 0)
			itemHeight = SchemeInt(scheme, "MainMenu.MenuItemHeight", kFallbackItemHeight);
		SetMenuItemHeight(itemHeight);

		SetBgColor(Color(0, 0, 0, 0));
		SetBorder(nullptr);
		SetPaintBackgroundEnabled(false);
	}

	void LayoutMenuBorder() override {}

	// Ein Popup-Menü schließt sich selbst: beim Aktivieren eines Eintrags, bei ESC und
	// bei Fokusverlust. Das Hauptmenü ist dauerhaft — es verschwindet nur, wenn wir es
	// über Hide() ausdrücklich verlangen. Sonst war es nach dem ersten Klick auf
	// „Options“ endgültig weg.
	void SetVisible(bool state) override
	{
		if (!state && !m_allowHide)
		{
			if (surface())
				surface()->MovePopupToBack(GetVPanel());
			return;
		}
		BaseClass::SetVisible(state);
	}

	void SetVisibleExplicit(bool state)
	{
		m_allowHide = true;
		SetVisible(state);
		m_allowHide = false;
	}

private:
	bool m_allowHide = false;
};

class CsretroMainMenu : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CsretroMainMenu, Panel);

public:
	explicit CsretroMainMenu(Panel *parent) : BaseClass(parent, "CsretroMainMenu")
	{
		SetPaintBackgroundEnabled(false); // engine draws the background layout
		SetPaintBorderEnabled(false);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);

		m_menu = new CsretroGameMenu(this, "GameMenu");
		BuildItems();
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		m_inset = SchemeInt(scheme, "InGameDesktop/GameMenuInset", kFallbackInset);
		if (m_inset <= 0)
			m_inset = kFallbackInset;
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		if (!m_menu)
			return;

		int screenWide = 640, screenTall = 480;
		if (surface())
			surface()->GetScreenSize(screenWide, screenTall);
		SetBounds(0, 0, screenWide, screenTall);

		m_menu->InvalidateLayout(true);
		int menuWide = 0, menuTall = 0;
		m_menu->GetSize(menuWide, menuTall);

		// Classic anchor: bottom-left, one inset above the lower screen edge.
		const int x = m_inset;
		int y = screenTall - menuTall - m_inset;
		if (y < 0)
			y = 0;
		m_menu->SetPos(x, y);

		if (getenv("CSRETRO_MAINMENU_GATE"))
			ReportLayout(screenWide, screenTall, x, y, menuWide, menuTall);
	}

	void OnCommand(const char *command) override
	{
		if (!command || !*command)
			return;
		GameUI_RunMenuCommand(command);
	}

	void UpdateItemState()
	{
		if (!m_menu)
			return;
		const bool inGame = GameUI_IsClientInGame();
		for (int i = 0; i < m_menu->GetChildCount(); ++i)
		{
			MenuItem *item = dynamic_cast<MenuItem *>(m_menu->GetChild(i));
			if (!item)
				continue;
			KeyValues *data = item->GetUserData();
			if (!data)
				continue;
			const bool onlyInGame = data->GetInt("OnlyInGame") != 0;
			item->SetVisible(!onlyInGame || inGame);
		}
		m_menu->InvalidateLayout();
		InvalidateLayout();
	}

	CsretroGameMenu *GetMenu() { return m_menu; }

private:
	void ReportLayout(int screenWide, int screenTall, int x, int y, int menuWide, int menuTall)
	{
		Menu_Con("CSRETRO_MAINMENU_LAYOUT screen=%dx%d menu=%d,%d %dx%d inset=%d itemheight=%d",
			screenWide, screenTall, x, y, menuWide, menuTall, m_inset, m_menu->GetMenuItemHeight());

		int visible = 0;
		for (int i = 0; i < m_menu->GetChildCount(); ++i)
		{
			MenuItem *item = dynamic_cast<MenuItem *>(m_menu->GetChild(i));
			if (!item)
				continue;
			int ix = 0, iy = 0, iw = 0, ih = 0;
			item->GetBounds(ix, iy, iw, ih);
			char text[128] = {0};
			item->GetText(text, sizeof(text));
			const bool shown = item->IsVisible();
			if (shown)
				++visible;
			Menu_Con("CSRETRO_MAINMENU_ITEM cmd=%s text=\"%s\" visible=%d bounds=%d,%d %dx%d",
				item->GetName(), text, shown ? 1 : 0, ix, iy, iw, ih);
		}
		Menu_Con("CSRETRO_MAINMENU_VISIBLE %d", visible);
	}

	void BuildItems()
	{
		const std::vector<GameMenuItem> items = Menu_LoadGameMenu();
		if (items.empty())
		{
			Menu_Con("CSRETRO_MAINMENU_NO_ITEMS resource/GameMenu.res");
			return;
		}

		int added = 0;
		for (const GameMenuItem &entry : items)
		{
			// Spacer rows exist only to group the in-game block; a real Menu lays
			// items out itself, so they would just be dead clickable rows.
			if (entry.empty || entry.command.empty())
				continue;

			KeyValues *userData = new KeyValues("GameMenuItem");
			userData->SetInt("OnlyInGame", entry.onlyInGame ? 1 : 0);

			// Label mit führendem '#' unverändert weiterreichen: Label::SetText löst das
			// über g_pVGuiLocalize auf. Menu_L ist der Interim-Localizer und würde bei
			// einem Treffer-Fehler still das rohe Token anzeigen.
			CsretroGameMenuItem *item =
				new CsretroGameMenuItem(m_menu, entry.command.c_str(), entry.label.c_str());
			item->AddActionSignalTarget(this);
			item->SetCommand(entry.command.c_str());
			item->SetUserData(userData);
			userData->deleteThis();

			m_menu->AddMenuItem(item);
			++added;
		}
		Menu_Con("CSRETRO_MAINMENU_ITEMS %d", added);
	}

	CsretroGameMenu *m_menu = nullptr;
	int m_inset = kFallbackInset;
};

CsretroMainMenu *g_mainMenu = nullptr;

// Engine grabs the *next* rendered frame, so the gate shot has to wait for paint.
int g_gateShotFrame = -1;
} // namespace

bool MainMenu_Show(Panel *root)
{
	if (!root)
		return false;
	if (!g_mainMenu)
		g_mainMenu = new CsretroMainMenu(root);
	if (!g_mainMenu->GetMenu())
		return false;

	g_mainMenu->UpdateItemState();
	g_mainMenu->SetVisible(true);
	g_mainMenu->GetMenu()->SetVisibleExplicit(true);
	g_mainMenu->InvalidateLayout(true);
	// vgui2::Menu ist ein Popup. Das Hauptmenü ist Hintergrundmöbel — Dialoge liegen
	// immer davor, sonst fängt es z. B. den linken Resize-Griff der Options ab.
	if (surface())
		surface()->MovePopupToBack(g_mainMenu->GetMenu()->GetVPanel());
	if (getenv("CSRETRO_MAINMENU_GATE") && g_gateShotFrame < 0)
		g_gateShotFrame = 0;
	return true;
}

// Regression: das Hauptmenü muss Öffnen, Anklicken und Schließen der Options überleben.
// Früher war es nach dem Klick auf „Options“ dauerhaft weg — MenuItem-Aktivierung
// schließt ein Popup-Menü. Der Treiber geht bewusst über UI_MouseMove/UI_KeyEvent,
// weil ein Aufruf der VGUI-Interna genau diesen Pfad verfehlt hat.
void MainMenu_GateTick()
{
	if (g_gateShotFrame < 0)
		return;
	const int frame = ++g_gateShotFrame;

	if (getenv("CSRETRO_MAINMENU_TRACE") && g_mainMenu)
	{
		static int s_prev = -1;
		const int now = g_mainMenu->GetMenu()->IsVisible() ? 1 : 0;
		if (now != s_prev)
		{
			s_prev = now;
			Menu_Con("CSRETRO_MAINMENU_TRACE frame=%d menuvisible=%d", frame, now);
		}
	}

	if (frame == 30)
	{
		if (gEng.pfnClientCmd)
			gEng.pfnClientCmd(0, "screenshot\n");
		Menu_Con("CSRETRO_MAINMENU_SHOT_TAKEN");
	}
	else if (frame == 45)
	{
		// Auf den Options-Eintrag zeigen und klicken — wie mit der Maus.
		MenuItem *options = nullptr;
		for (int i = 0; g_mainMenu && i < g_mainMenu->GetMenu()->GetChildCount(); ++i)
		{
			MenuItem *item = dynamic_cast<MenuItem *>(g_mainMenu->GetMenu()->GetChild(i));
			if (item && item->IsVisible() && !strcmp(item->GetName(), "OpenOptionsDialog"))
				options = item;
		}
		if (!options)
		{
			Menu_Con("CSRETRO_MAINMENU_SURVIVE_OPEN options=0 menu=0 (Eintrag fehlt)");
		}
		else
		{
			int cx = options->GetWide() / 2, cy = options->GetTall() / 2;
			options->LocalToScreen(cx, cy);
			UI_MouseMove(cx, cy);
			UI_KeyEvent(K_MOUSE1, 1);
			UI_KeyEvent(K_MOUSE1, 0);
		}
	}
	else if (frame == 55)
	{
		// ActionSignals laufen asynchron — erst jetzt auswerten.
		Menu_Con("CSRETRO_MAINMENU_SURVIVE_OPEN options=%d menu=%d",
			VGuiXash_IsOptionsActive() ? 1 : 0,
			g_mainMenu && g_mainMenu->GetMenu()->IsVisible() ? 1 : 0);
	}
	else if (frame == 65)
	{
		// Klick in die Bildschirmmitte: im Dialog, außerhalb des Hauptmenüs.
		int sw = 640, sh = 480;
		if (surface())
			surface()->GetScreenSize(sw, sh);
		UI_MouseMove(sw / 2, sh / 2);
		UI_KeyEvent(K_MOUSE1, 1);
		UI_KeyEvent(K_MOUSE1, 0);
		Menu_Con("CSRETRO_MAINMENU_SURVIVE_CLICK %d", MainMenu_IsActive() ? 1 : 0);
	}
	else if (frame == 75)
	{
		UI_KeyEvent(K_ESCAPE, 1);
		UI_KeyEvent(K_ESCAPE, 0);
		Menu_Con("CSRETRO_MAINMENU_SURVIVE_ESC options=%d menu=%d",
			VGuiXash_IsOptionsActive() ? 1 : 0, MainMenu_IsActive() ? 1 : 0);
	}
	else if (frame >= 90)
	{
		g_gateShotFrame = -1;
		const bool panelVisible = MainMenu_IsActive();
		const bool menuVisible = g_mainMenu && g_mainMenu->GetMenu()->IsVisible();
		Menu_Con("CSRETRO_MAINMENU_SURVIVE panel=%d menu=%d", panelVisible ? 1 : 0, menuVisible ? 1 : 0);
		Menu_Con("CSRETRO_MAINMENU_GATE_DONE");
	}
}

void MainMenu_Hide()
{
	if (!g_mainMenu)
		return;
	g_mainMenu->GetMenu()->SetVisibleExplicit(false);
	g_mainMenu->SetVisible(false);
}

bool MainMenu_IsActive()
{
	return g_mainMenu && g_mainMenu->IsVisible();
}

void MainMenu_UpdateItemState()
{
	if (g_mainMenu)
		g_mainMenu->UpdateItemState();
}

void MainMenu_InvalidateLayout()
{
	if (g_mainMenu)
		g_mainMenu->InvalidateLayout();
}

void MainMenu_Shutdown()
{
	// Parented to the VGUI root, which deletes the tree; just drop our handle.
	g_mainMenu = nullptr;
}
