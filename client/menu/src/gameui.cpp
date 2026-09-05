#include "menu_priv.h"

#include "keydefs.h"
#include "../vgui/vgui_boot.h"
#include "../gameui/OptionsKeyboardGate.h"
#include "../vgui/menu_runtime_info.h"

// Nur die Modulschnittstelle — den VGUI-Header hier einzuziehen würde
// xash3d_types.h und SDK-Macros (SetBits/LittleLong/…) kollidieren lassen.
void ServerBrowser_AddFromEngine(const char *address, const char *info);

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>
#include <unistd.h>

extern char **environ;

// Xash keydest_t: key_console=0, key_game=1, key_menu=2
#ifndef KEY_DEST_CONSOLE
#define KEY_DEST_CONSOLE 0
#endif
#ifndef KEY_DEST_GAME
#define KEY_DEST_GAME 1
#endif
#ifndef KEY_DEST_MENU
#define KEY_DEST_MENU 2
#endif

MenuScreen gScreen = SCREEN_MAIN;
bool gMenuVisible = false;
int gMouseX, gMouseY;
ServerProfile gProfile;

static std::vector<ResField> gInGame;
static int gInGameType = 0;
static bool gInGameOn = false;
static int gLastTeam = 0;

void Menu_NotePlayerTeam(int team)
{
	if (team == 1 || team == 2)
		gLastTeam = team;
}

int Menu_LastPlayerTeam()
{
	return gLastTeam;
}

// Ein Motiv, kein Steam-Kachelset. Engine-PIC_Load (RODIR), nicht VGUI-IFileSystem.
static const char kBackgroundPath[] = "resource/background/csretro.png";
static HIMAGE gBgPic = 0;
static int gBgSrcW = 1, gBgSrcH = 1;

static bool MenuConToEngine()
{
	// play.sh: nur stderr, nie Notify-HUD. Gates brauchen engine.log (-log).
	if (getenv("CSRETRO_MENU_CON_ENGINE") || getenv("CSRETRO_V1POC") || getenv("CSRETRO_OPTIONS_AUTO") ||
		getenv("CSRETRO_CONSOLE_DEBUG"))
		return true;
	for (char **e = environ; e && *e; ++e)
	{
		if (strncmp(*e, "CSRETRO_", 8) != 0)
			continue;
		if (strstr(*e, "_GATE="))
			return true;
	}
	return false;
}

void Menu_Con(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fputs(buf, stderr);
	fputc('\n', stderr);
	fflush(stderr);
	if (MenuConToEngine() && gEng.Con_Printf)
		gEng.Con_Printf("%s\n", buf);
}

void Menu_DrawText(int x, int y, const char *text, int r, int g, int b, int a)
{
	if (!text || !text[0])
		return;
	gEng.pfnDrawSetTextColor(r, g, b, a);
	gEng.pfnDrawConsoleString(x, y, text);
}

bool Menu_Hit(int mx, int my, int x, int y, int w, int h)
{
	return mx >= x && my >= y && mx < x + w && my < y + h;
}

void Menu_LoadBackground()
{
	gBgPic = 0;
	gBgSrcW = 1;
	gBgSrcH = 1;
	if (!gEng.pfnPIC_Load)
	{
		Menu_Con("CSRETRO_BG FAIL %s", kBackgroundPath);
		return;
	}
	const HIMAGE pic = gEng.pfnPIC_Load(kBackgroundPath, nullptr, 0, 0);
	if (!pic)
	{
		Menu_Con("CSRETRO_BG FAIL %s", kBackgroundPath);
		return;
	}
	gBgPic = pic;
	gBgSrcW = std::max(1, gEng.pfnPIC_Width(pic));
	gBgSrcH = std::max(1, gEng.pfnPIC_Height(pic));
	Menu_Con("CSRETRO_BG %s %dx%d", kBackgroundPath, gBgSrcW, gBgSrcH);
}

void Menu_DrawBackground()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	gEng.pfnFillRGBA(0, 0, sw, sh, 0, 0, 0, 255);
	if (!gBgPic || gBgSrcW < 1 || gBgSrcH < 1)
		return;
	// Cover: Seitenverhältnis halten, Überstand mittig abschneiden (kein Verzerren).
	const float sx = static_cast<float>(sw) / static_cast<float>(gBgSrcW);
	const float sy = static_cast<float>(sh) / static_cast<float>(gBgSrcH);
	const float scale = std::max(sx, sy);
	const float visW = static_cast<float>(sw) / scale;
	const float visH = static_cast<float>(sh) / scale;
	const float srcX = (static_cast<float>(gBgSrcW) - visW) * 0.5f;
	const float srcY = (static_cast<float>(gBgSrcH) - visH) * 0.5f;
	wrect_t rc;
	rc.left = std::max(0, static_cast<int>(srcX));
	rc.top = std::max(0, static_cast<int>(srcY));
	rc.right = std::min(gBgSrcW, static_cast<int>(srcX + visW));
	rc.bottom = std::min(gBgSrcH, static_cast<int>(srcY + visH));
	if (rc.right <= rc.left || rc.bottom <= rc.top)
		return;
	gEng.pfnPIC_Set(gBgPic, 255, 255, 255, 255);
	gEng.pfnPIC_Draw(0, 0, sw, sh, &rc);
}

static bool InGame()
{
	return gEng.pfnClientInGame && gEng.pfnClientInGame();
}

static const char *InGameResPath(int menuType)
{
	switch (menuType)
	{
	case 2:
		return "resource/UI/Teammenu.res";
	case 26:
		return "resource/UI/Classmenu_TER.res";
	case 27:
		return "resource/UI/Classmenu_CT.res";
	case 28:
		return "resource/UI/MainBuyMenu.res";
	case 29:
		return InGame() ? "resource/UI/BuyPistols_CT.res" : "resource/UI/BuyPistols_TER.res";
	case 30:
		return "resource/UI/BuyShotguns_TER.res";
	case 31:
		return "resource/UI/BuySubMachineguns_TER.res";
	case 32:
		return "resource/UI/BuyRifles_TER.res";
	case 33:
		return "resource/UI/BuyMachineguns_TER.res";
	case 34:
		return "resource/UI/BuyEquipment.res";
	default:
		return nullptr;
	}
}

void GameUI_ShowInGame(int menuType, int validSlots)
{
	Menu_Con("CSRETRO_VGUI_SHOW type=%d slots=%d", menuType, validSlots);
	gInGameType = menuType;
	gInGameOn = false;
	gInGame.clear();
	if (menuType == 2)
	{
		VGuiXash_HideClassSelect();
		VGuiXash_HideBuySelect();
		VGuiXash_HideRadioSelect();
		if (VGuiXash_ShowTeamSelect(validSlots))
			return;
		Menu_Con("CSRETRO_TEAM_VGUI fail — Interim");
	}
	else if (menuType == 26 || menuType == 27)
	{
		VGuiXash_HideTeamSelect();
		VGuiXash_HideBuySelect();
		VGuiXash_HideRadioSelect();
		Menu_NotePlayerTeam(menuType == 27 ? 2 : 1);
		if (VGuiXash_ShowClassSelect(menuType, validSlots))
			return;
		Menu_Con("CSRETRO_CLASS_VGUI fail — Interim");
	}
	else if (menuType >= 28 && menuType <= 34)
	{
		VGuiXash_HideTeamSelect();
		VGuiXash_HideClassSelect();
		VGuiXash_HideRadioSelect();
		if (VGuiXash_ShowBuySelect(menuType, validSlots))
			return;
		Menu_Con("CSRETRO_BUY_VGUI fail — Interim");
	}
	else if (menuType >= 35 && menuType <= 37)
	{
		VGuiXash_HideTeamSelect();
		VGuiXash_HideClassSelect();
		VGuiXash_HideBuySelect();
		if (VGuiXash_ShowRadioSelect(menuType, validSlots))
			return;
		Menu_Con("CSRETRO_RADIO_VGUI fail — ShowMenu-Legacy");
	}
	else
	{
		VGuiXash_HideTeamSelect();
		VGuiXash_HideClassSelect();
		VGuiXash_HideBuySelect();
		VGuiXash_HideRadioSelect();
	}

	const char *path = InGameResPath(menuType);
	if (!path)
	{
		Menu_Con("CSRetro-VGUI: kein .res für Menü %d — ShowMenu-Legacy", menuType);
		return;
	}
	gInGame = Menu_LoadRes(path);
	if (gInGame.empty())
	{
		Menu_Con("CSRetro-VGUI: %s leer", path);
		return;
	}
	gInGameOn = true;
	Menu_Con("CSRetro-VGUI: %s (%d)", path, menuType);
}

void GameUI_HideInGame()
{
	VGuiXash_HideTeamSelect();
	VGuiXash_HideClassSelect();
	VGuiXash_HideBuySelect();
	VGuiXash_HideRadioSelect();
	gInGameOn = false;
	gInGame.clear();
}

bool GameUI_InGameActive()
{
	return gInGameOn || VGuiXash_IsTeamSelectActive() || VGuiXash_IsClassSelectActive() ||
		VGuiXash_IsBuySelectActive() || VGuiXash_IsRadioSelectActive();
}

static void ScaleRect(int *x, int *y, int *w, int *h)
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	const float s = std::min(sw / 640.f, sh / 480.f);
	const int ox = (sw - static_cast<int>(640 * s)) / 2;
	const int oy = (sh - static_cast<int>(480 * s)) / 2;
	*x = ox + static_cast<int>(*x * s);
	*y = oy + static_cast<int>(*y * s);
	*w = std::max(1, static_cast<int>(*w * s));
	*h = std::max(1, static_cast<int>(*h * s));
}

static std::vector<ResField *> VisibleButtons()
{
	std::vector<ResField *> out;
	for (ResField &f : gInGame)
	{
		if (!f.visible || !f.enabled)
			continue;
		if (strcasecmp(f.control.c_str(), "Button") != 0 && strcasecmp(f.control.c_str(), "MouseOverPanelButton") != 0)
			continue;
		if (f.command.empty())
			continue;
		out.push_back(&f);
	}
	return out;
}

static bool LoadInGameResFile(const char *path)
{
	std::vector<ResField> fields = Menu_LoadRes(path);
	if (fields.empty())
		return false;
	gInGame = std::move(fields);
	gInGameOn = true;
	Menu_Con("CSRetro-VGUI: %s", path);
	return true;
}

static bool OpenResCommand(const char *cmd)
{
	char path[256];
	snprintf(path, sizeof(path), "%s", cmd);
	for (char *p = path; *p; ++p)
	{
		if (*p == '\\')
			*p = '/';
	}
	if (!strncasecmp(path, "Resource/", 9))
		path[0] = 'r';

	if (LoadInGameResFile(path))
		return true;

	char stem[256];
	snprintf(stem, sizeof(stem), "%s", path);
	char *dot = strstr(stem, ".res");
	if (!dot)
		return false;
	*dot = '\0';

	char tryPath[280];
	snprintf(tryPath, sizeof(tryPath), "%s_CT.res", stem);
	if (LoadInGameResFile(tryPath))
		return true;
	snprintf(tryPath, sizeof(tryPath), "%s_TER.res", stem);
	if (LoadInGameResFile(tryPath))
		return true;

	char *guns = strstr(stem, "MachineGuns");
	if (guns)
	{
		memcpy(guns, "Machineguns", 11);
		snprintf(tryPath, sizeof(tryPath), "%s_CT.res", stem);
		if (LoadInGameResFile(tryPath))
			return true;
		snprintf(tryPath, sizeof(tryPath), "%s_TER.res", stem);
		if (LoadInGameResFile(tryPath))
			return true;
	}
	return false;
}

bool GameUI_ActivateSlot(int slot)
{
	if (VGuiXash_IsTeamSelectActive())
		return VGuiXash_TeamActivateSlot(slot);
	if (VGuiXash_IsClassSelectActive())
		return VGuiXash_ClassActivateSlot(slot);
	if (VGuiXash_IsBuySelectActive())
		return VGuiXash_BuyActivateSlot(slot);
	if (VGuiXash_IsRadioSelectActive())
		return VGuiXash_RadioActivateSlot(slot);
	if (!gInGameOn)
		return false;
	auto btns = VisibleButtons();
	ResField *pick = nullptr;
	for (ResField *f : btns)
	{
		const char *sp = strrchr(f->command.c_str(), ' ');
		if (sp && atoi(sp + 1) == slot)
		{
			pick = f;
			break;
		}
	}
	if (!pick && slot >= 1 && slot <= static_cast<int>(btns.size()))
		pick = btns[static_cast<size_t>(slot - 1)];
	if (!pick)
		return false;
	const char *cmd = pick->command.c_str();
	if (!strcasecmp(cmd, "vguicancel"))
	{
		GameUI_HideInGame();
		return true;
	}
	if (strstr(cmd, ".res") || strstr(cmd, ".RES"))
		return OpenResCommand(cmd);
	Menu_Con("CSRetro-VGUI: slot %d → %s", slot, cmd);
	gEng.pfnClientCmd(1, cmd);
	GameUI_HideInGame();
	return true;
}

void GameUI_InGameKey(int key, int down)
{
	if (VGuiXash_IsRadioSelectActive() && !VGuiXash_IsTeamSelectActive() &&
		!VGuiXash_IsClassSelectActive() && !VGuiXash_IsBuySelectActive())
	{
		if (!down)
			return;
		if (key == K_ESCAPE)
		{
			VGuiXash_HideRadioSelect();
			return;
		}
		if (key >= '1' && key <= '9')
		{
			VGuiXash_RadioActivateSlot(key - '0');
			return;
		}
		if (key == '0')
			VGuiXash_RadioActivateSlot(10);
		return;
	}
	if (VGuiXash_IsTeamSelectActive() || VGuiXash_IsClassSelectActive() ||
		VGuiXash_IsBuySelectActive())
	{
		const bool mouse = (key >= K_MOUSE1 && key <= K_MOUSE5) ||
			key == K_MWHEELUP || key == K_MWHEELDOWN;
		if (mouse)
		{
			VGuiXash_Key(key, down);
			return;
		}
		if (!down)
			return;
		if (key == K_ESCAPE)
		{
			if (VGuiXash_IsBuySelectActive())
				VGuiXash_HideBuySelect();
			else if (VGuiXash_IsClassSelectActive())
				VGuiXash_HideClassSelect();
			else
				VGuiXash_HideTeamSelect();
			return;
		}
		if (key >= '1' && key <= '9')
		{
			if (VGuiXash_IsBuySelectActive())
				VGuiXash_BuyActivateSlot(key - '0');
			else if (VGuiXash_IsClassSelectActive())
				VGuiXash_ClassActivateSlot(key - '0');
			else
				VGuiXash_TeamActivateSlot(key - '0');
			return;
		}
		if (key == '0')
		{
			if (VGuiXash_IsBuySelectActive())
				VGuiXash_BuyActivateSlot(10);
			else if (VGuiXash_IsClassSelectActive())
				VGuiXash_ClassActivateSlot(10);
			else
				VGuiXash_TeamActivateSlot(10);
			return;
		}
		// A/R Autobuy/Rebuy und sonstige Tasten an das Overlay.
		VGuiXash_Key(key, down);
		return;
	}
	if (!down || !gInGameOn)
		return;
	if (key == K_ESCAPE)
	{
		GameUI_HideInGame();
		return;
	}
	if (key >= '1' && key <= '9')
		GameUI_ActivateSlot(key - '0');
	else if (key == '0')
		GameUI_ActivateSlot(10);
}

void GameUI_InGameDraw()
{
	if (!gInGameOn)
		return;
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	gEng.pfnFillRGBA(0, 0, sw, sh, 0, 0, 0, 160);

	for (ResField &f : gInGame)
	{
		if (!f.visible)
			continue;
		int x = f.x, y = f.y, w = f.w, h = f.h;
		ScaleRect(&x, &y, &w, &h);
		const bool btn = !strcasecmp(f.control.c_str(), "Button") || !strcasecmp(f.control.c_str(), "MouseOverPanelButton");
		const bool hover = btn && Menu_Hit(gMouseX, gMouseY, x, y, w, h);
		if (btn)
			gEng.pfnFillRGBA(x, y, w, h, 0, 0, 0, hover ? 180 : 120);
		if (!f.label.empty())
		{
			const int r = hover ? 255 : 255;
			const int g = hover ? 220 : 176;
			const int b = hover ? 80 : 0;
			Menu_DrawText(x + 4, y + std::max(0, (h - 14) / 2), Menu_L(f.label.c_str()), r, g, b, 255);
		}
	}
}

bool GameUI_IsClientInGame()
{
	return InGame();
}

void GameUI_RunMenuCommand(const char *command)
{
	const std::string cmd = command ? command : "";
	if (cmd == "ResumeGame")
	{
		VGuiXash_HideMainMenu();
		gMenuVisible = false;
		gEng.pfnSetKeyDest(KEY_DEST_GAME);
		PauseBackdrop_Invalidate();
		Menu_Con("CSRETRO_PAUSE_VGUI close");
	}
	else if (cmd == "Disconnect")
		gEng.pfnClientCmd(0, "disconnect\n");
	else if (cmd == "Quit")
		gEng.pfnClientCmd(0, "quit\n");
	else if (cmd == "OpenOptionsDialog")
		GameUI_OpenOptions();
	else if (cmd == "OpenCreateMultiplayerGameDialog")
		GameUI_OpenNewGame();
	else if (cmd == "OpenServerBrowser")
		GameUI_OpenBrowser();
	else if (cmd == "OpenPlayerListDialog")
		gEng.pfnClientCmd(0, "menu_playerlist\n");
	else if (!cmd.empty())
		gEng.pfnClientCmd(0, (cmd + "\n").c_str());
}

void GameUI_OpenOptions()
{
	gScreen = SCREEN_OPTIONS;
	gMenuVisible = true;
	if (gEng.pfnSetKeyDest)
		gEng.pfnSetKeyDest(KEY_DEST_MENU);
	if (getenv("CSRETRO_V1POC"))
	{
		VGuiXash_ShowPocDialog();
		return;
	}
	// Der Dialog liegt vor dem Hauptmenü — das bleibt sichtbar wie im Original.
	if (!VGuiXash_ShowOptionsDialog())
		Menu_Con("CSRETRO_OPTIONS_INTERIM");
}

void GameUI_OpenNewGame()
{
	gScreen = SCREEN_NEWGAME;
	// Der Dialog liegt vor dem Hauptmenü, wie Options.
	if (VGuiXash_ShowCreateGameDialog())
		return;
	// Ohne Maps kein Dialog — dann bleibt das Hauptmenü stehen statt einer leeren Combo.
	gScreen = SCREEN_MAIN;
	Menu_Con("CSRETRO_CREATE_UNAVAILABLE");
}

void GameUI_OpenBrowser()
{
	gScreen = SCREEN_BROWSER;
	if (VGuiXash_ShowServerBrowser())
		return;
	gScreen = SCREEN_MAIN;
	Menu_Con("CSRETRO_BROWSER_UNAVAILABLE");
}

int UI_VidInit(void)
{
	Menu_LoadLocale();
	Menu_LoadBackground();
	Profile_Defaults(&gProfile);
	return 1;
}

void UI_Init(void)
{
	gEng.Con_Printf("CS Retro Menu: GetMenuAPI + V1 VGUI + GameMenuExports001\n");
	CsretroMenu_LogProvenance("UI_Init");
	VGuiXash_Init();
	// Auto-Show läuft über UI_SetActiveMenu(1) wenn CSRETRO_V1POC gesetzt ist.
}

void UI_Shutdown(void)
{
	VGuiXash_Shutdown();
	gBgPic = 0;
	GameUI_HideInGame();
}

void UI_Redraw(float)
{
	// VGUI2 windows are also painted over a running game when the main menu is hidden.
	if (VGuiXash_IsUiActive())
	{
		if (gMenuVisible && !VGuiXash_IsTeamSelectActive() && !VGuiXash_IsClassSelectActive() &&
			!VGuiXash_IsBuySelectActive() && !VGuiXash_IsRadioSelectActive() &&
			!VGuiXash_IsSpectatorActive() && !VGuiXash_IsScoreboardActive())
		{
			if (InGame())
			{
				// Calibrate against the current rendered world. A cached, darkened
				// pause image hides lightmap changes and misrepresents brightness.
				if (VGuiXash_IsVideoCalibrationActive())
					PauseBackdrop_Invalidate();
				else
					PauseBackdrop_Paint();
			}
			else
				Menu_DrawBackground();
		}
		VGuiXash_RunFrame();
		VGuiXash_Paint();
		return;
	}
	// Gate muss auch nach ESC weiterticken, sonst hängt Reopen/Join.
	if (getenv("CSRETRO_TEAM_GATE") || getenv("CSRETRO_CLASS_GATE") || getenv("CSRETRO_BUY_GATE") ||
		getenv("CSRETRO_RADIO_GATE") || getenv("CSRETRO_PAUSE_GATE") || getenv("CSRETRO_SPEC_GATE") ||
		getenv("CSRETRO_SCORE_GATE"))
		VGuiXash_RunFrame();
	if (gInGameOn)
	{
		GameUI_InGameDraw();
		return;
	}
	if (!gMenuVisible)
		return;
	Menu_DrawBackground();
}

void UI_KeyEvent(int key, int down)
{
	if (VGuiXash_IsTeamSelectActive() || VGuiXash_IsClassSelectActive() ||
		VGuiXash_IsBuySelectActive())
	{
		GameUI_InGameKey(key, down);
		return;
	}
	if (VGuiXash_IsRadioSelectActive())
	{
		if (key == K_ESCAPE || (key >= '0' && key <= '9'))
			GameUI_InGameKey(key, down);
		return;
	}
	if (VGuiXash_IsInteractiveUiActive())
	{
		// ESC during keyboard capture: cancel capture only — never close Options.
		if (down && key == K_ESCAPE && VGuiXash_IsKeyboardCapturing())
		{
			VGuiXash_Key(key, down);
			return;
		}
		// Physical capture probe: keep Options open until SDL key arrives.
		if (down && key == K_ESCAPE && getenv("CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS") &&
			OptionsKeyboard_PhysicalCaptureProbeArmed())
		{
			VGuiXash_Key(key, down); // cancel capture if any; do not hide dialog
			return;
		}
		VGuiXash_Key(key, down);
		if (down && key == K_ESCAPE)
		{
			if (VGuiXash_IsTeamSelectActive())
			{
				VGuiXash_HideTeamSelect();
				return;
			}
			if (VGuiXash_IsClassSelectActive())
			{
				VGuiXash_HideClassSelect();
				return;
			}
			if (VGuiXash_IsBuySelectActive())
			{
				VGuiXash_HideBuySelect();
				return;
			}
			if (VGuiXash_IsRadioSelectActive())
			{
				VGuiXash_HideRadioSelect();
				return;
			}
			if (VGuiXash_IsConsoleActive())
			{
				VGuiXash_HideConsole();
				return;
			}
			const bool hadDialog = VGuiXash_IsPocActive() || VGuiXash_IsOptionsActive() ||
				VGuiXash_IsCreateGameActive() || VGuiXash_IsServerBrowserActive();
			if (VGuiXash_IsPocActive())
				VGuiXash_HidePocDialog();
			if (VGuiXash_IsOptionsActive())
				VGuiXash_HideOptionsDialog();
			if (VGuiXash_IsCreateGameActive())
				VGuiXash_HideCreateGameDialog();
			if (VGuiXash_IsServerBrowserActive())
				VGuiXash_HideServerBrowser();
			gScreen = SCREEN_MAIN;
			// ESC im blanken Hauptmenü heißt „zurück ins Spiel“, wie im Original.
			if (!hadDialog && VGuiXash_IsMainMenuActive() && InGame())
			{
				VGuiXash_HideMainMenu();
				gMenuVisible = false;
				gEng.pfnSetKeyDest(KEY_DEST_GAME);
				PauseBackdrop_Invalidate();
				Menu_Con("CSRETRO_PAUSE_VGUI close");
			}
		}
		return;
	}
	if (!down)
		return;
	if (gInGameOn)
	{
		GameUI_InGameKey(key, down);
		return;
	}
	if (!gMenuVisible)
		return;
	if (key == K_ESCAPE)
	{
		if (gScreen != SCREEN_MAIN)
		{
			gScreen = SCREEN_MAIN;
			VGuiXash_ShowMainMenu();
		}
		else if (InGame())
		{
			gMenuVisible = false;
			gEng.pfnSetKeyDest(KEY_DEST_GAME);
			PauseBackdrop_Invalidate();
			Menu_Con("CSRETRO_PAUSE_VGUI close");
		}
		return;
	}
}

void UI_MouseMove(int x, int y)
{
	gMouseX = x;
	gMouseY = y;
	if (VGuiXash_IsInteractiveUiActive())
		VGuiXash_MouseMove(x, y);
}

void UI_SetActiveMenu(int active)
{
	gMenuVisible = active != 0;
	if (gMenuVisible)
	{
		gScreen = SCREEN_MAIN;
		gEng.pfnSetKeyDest(KEY_DEST_MENU);
		VGuiXash_HideTeamSelect();
		VGuiXash_HideClassSelect();
		VGuiXash_HideBuySelect();
		VGuiXash_HideRadioSelect();
		VGuiXash_HideSpectatorHud();
		VGuiXash_HideScoreboardHud();
		if (getenv("CSRETRO_V1POC"))
			VGuiXash_ShowPocDialog();
		else
			VGuiXash_ShowMainMenu();
		if (InGame())
			Menu_Con("CSRETRO_PAUSE_VGUI open wallpaper=0 blur=1");
		return;
	}
	PauseBackdrop_Invalidate();
	VGuiXash_HideMainMenu();
	// Wie Xash-MainUI UI_CloseMenu: In-Game-VGUI ist Overlay, nicht key_menu.
	if (!gInGameOn && !VGuiXash_IsTeamSelectActive() && !VGuiXash_IsClassSelectActive() &&
		!VGuiXash_IsBuySelectActive())
		gEng.pfnSetKeyDest(KEY_DEST_GAME);
}

void UI_AddServerToList(struct netadr_s adr, const char *info)
{
	if (!gExtEngReady || !gExtEng.pfnAdrToString)
		return;
	ServerBrowser_AddFromEngine(gExtEng.pfnAdrToString(adr), info);
}

void UI_GetCursorPos(int *x, int *y)
{
	if (x)
		*x = gMouseX;
	if (y)
		*y = gMouseY;
}

void UI_SetCursorPos(int x, int y)
{
	gMouseX = x;
	gMouseY = y;
}

void UI_ShowCursor(int)
{
}

void UI_CharEvent(int key)
{
	if (VGuiXash_IsUiActive())
		VGuiXash_Char(key);
}

int UI_MouseInRect(void)
{
	return 1;
}

int UI_IsVisible(void)
{
	return (gMenuVisible || VGuiXash_IsConsoleActive()) ? 1 : 0;
}

int UI_CreditsActive(void)
{
	return 0;
}

void UI_FinalCredits(void)
{
}
