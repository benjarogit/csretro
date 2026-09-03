#include "menu_priv.h"

#include "keydefs.h"
#include "../vgui/vgui_boot.h"
#include "../gameui/OptionsKeyboardGate.h"
#include "../vgui/menu_runtime_info.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>

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

static std::vector<GameMenuItem> gMainItems;
static std::vector<ResField> gInGame;
static int gInGameType = 0;
static bool gInGameOn = false;
static int gHoverMain = -1;

struct BgTile
{
	HIMAGE pic = 0;
	int x = 0, y = 0, fitW = 256, fitH = 256;
};
static std::vector<BgTile> gBg;
static int gBgSrcW = 800, gBgSrcH = 600;

void Menu_Con(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (gEng.Con_Printf)
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
	gBg.clear();
	int len = 0;
	byte *raw = gEng.COM_LoadFile("resource/BackgroundLayout.txt", &len);
	if (!raw)
		raw = gEng.COM_LoadFile("resource/background/BackgroundLayout.txt", &len);
	if (!raw || len <= 0)
		return;
	char *p = reinterpret_cast<char *>(raw);
	char token[256];
	int mode = 0;
	while ((p = gEng.COM_ParseFile(p, token)) != nullptr)
	{
		if (!strcmp(token, "resolution") && p)
		{
			p = gEng.COM_ParseFile(p, token);
			if (p)
				gBgSrcW = atoi(token);
			p = gEng.COM_ParseFile(p, token);
			if (p)
				gBgSrcH = atoi(token);
			continue;
		}
		if (strstr(token, ".tga") || strstr(token, ".TGA"))
		{
			BgTile t;
			t.pic = gEng.pfnPIC_Load(token, nullptr, 0, PIC_NOFLIP_TGA);
			char how[32] = {};
			p = gEng.COM_ParseFile(p, how);
			p = gEng.COM_ParseFile(p, token);
			if (p)
				t.x = atoi(token);
			p = gEng.COM_ParseFile(p, token);
			if (p)
				t.y = atoi(token);
			t.fitW = gEng.pfnPIC_Width(t.pic);
			t.fitH = gEng.pfnPIC_Height(t.pic);
			if (t.pic)
				gBg.push_back(t);
			(void)mode;
		}
	}
	gEng.COM_FreeFile(raw);
}

void Menu_DrawBackground()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	gEng.pfnFillRGBA(0, 0, sw, sh, 0, 0, 0, 255);
	if (gBg.empty() || gBgSrcW < 1 || gBgSrcH < 1)
		return;
	const float sx = static_cast<float>(sw) / static_cast<float>(gBgSrcW);
	const float sy = static_cast<float>(sh) / static_cast<float>(gBgSrcH);
	for (const BgTile &t : gBg)
	{
		if (!t.pic)
			continue;
		const int x = static_cast<int>(t.x * sx);
		const int y = static_cast<int>(t.y * sy);
		const int w = std::max(1, static_cast<int>(t.fitW * sx));
		const int h = std::max(1, static_cast<int>(t.fitH * sy));
		gEng.pfnPIC_Set(t.pic, 255, 255, 255, 255);
		gEng.pfnPIC_Draw(x, y, w, h, nullptr);
	}
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

void GameUI_ShowInGame(int menuType)
{
	const char *path = InGameResPath(menuType);
	gInGameType = menuType;
	gInGameOn = false;
	gInGame.clear();
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
	gInGameOn = false;
	gInGame.clear();
}

bool GameUI_InGameActive()
{
	return gInGameOn;
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

static void RunMainCommand(const std::string &cmd)
{
	if (cmd == "ResumeGame")
	{
		gMenuVisible = false;
		gEng.pfnSetKeyDest(KEY_DEST_GAME);
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
	// Echter Options-Dialog nur mit registrierten Subpages; sonst Interim-DrawOptions.
	if (!VGuiXash_ShowOptionsDialog())
		Menu_Con("CSRETRO_OPTIONS_INTERIM");
}

void GameUI_OpenNewGame()
{
	gScreen = SCREEN_NEWGAME;
	Profile_Defaults(&gProfile);
}

void GameUI_OpenBrowser()
{
	gScreen = SCREEN_BROWSER;
}

static void DrawFrame(const char *title, int x, int y, int w, int h)
{
	gEng.pfnFillRGBA(x, y, w, h, 0, 0, 0, 200);
	gEng.pfnFillRGBA(x, y, w, 22, 0, 0, 0, 230);
	Menu_DrawText(x + 8, y + 4, title, 255, 174, 0, 255);
}

static void DrawOptions()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	const int w = 420, h = 280;
	const int x = (sw - w) / 2, y = (sh - h) / 2;
	DrawFrame(Menu_L("GameUI_Options"), x, y, w, h);
	Menu_DrawText(x + 16, y + 36, "Multiplayer / Keyboard / Mouse / Audio / Video / Voice", 255, 176, 0, 255);
	Menu_DrawText(x + 16, y + 56, "CS Retro / Game — erst mit vorhandenem Backend.", 188, 112, 0, 255);

	const char *rows[][2] = {
		{ "sensitivity", "Mouse" },
		{ "volume", "Audio" },
		{ "name", "Multiplayer" },
	};
	int yy = y + 90;
	for (auto &row : rows)
	{
		const char *val = gEng.pfnGetCvarString(row[0]);
		char line[160];
		snprintf(line, sizeof(line), "%s  (%s)  %s", row[0], row[1], val ? val : "");
		Menu_DrawText(x + 16, yy, line, 255, 176, 0, 255);
		yy += 22;
	}
	Menu_DrawText(x + 16, y + h - 36, "ESC zurück — keine Placeholder-Feature-Schalter.", 255, 176, 0, 200);
}

static void DrawNewGame()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	const int w = 420, h = 300;
	const int x = (sw - w) / 2, y = (sh - h) / 2;
	DrawFrame(Menu_L("GameUI_CreateServer"), x, y, w, h);
	char line[192];
	snprintf(line, sizeof(line), "Server   Map %s   Host %s   Slots %d   LAN %d",
		gProfile.map.c_str(), gProfile.hostname.c_str(), gProfile.maxplayers, gProfile.lan);
	Menu_DrawText(x + 16, y + 40, line, 255, 176, 0, 255);
	snprintf(line, sizeof(line), "Game     Round %.1f  Freeze %.0f  FF %d  Balance %d",
		gProfile.roundtime, gProfile.freezetime, gProfile.friendlyfire, gProfile.teambalance);
	Menu_DrawText(x + 16, y + 64, line, 255, 176, 0, 255);
	snprintf(line, sizeof(line), "Bots     Quota %d  Diff %d  Team %s",
		gProfile.bot_quota, gProfile.bot_difficulty, gProfile.bot_join_team.c_str());
	Menu_DrawText(x + 16, y + 88, line, 255, 176, 0, 255);
	Menu_DrawText(x + 16, y + 112, "Modules  none", 255, 176, 0, 255);
	Menu_DrawText(x + 16, y + 160, "ENTER starten    ESC zurück", 255, 220, 80, 255);
}

static void DrawBrowser()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	const int w = 480, h = 240;
	const int x = (sw - w) / 2, y = (sh - h) / 2;
	DrawFrame(Menu_L("GameUI_GameMenu_FindServers"), x, y, w, h);
	Menu_DrawText(x + 16, y + 48, "Internet-Browser folgt (NextClient-Port, ohne Steam-Master).", 255, 176, 0, 255);
	Menu_DrawText(x + 16, y + 72, "LAN: Engine liefert Server über AddServerToList.", 255, 176, 0, 255);
	Menu_DrawText(x + 16, y + 120, "ESC zurück", 255, 220, 80, 255);
}

static void DrawMain()
{
	const int sw = gGlobals ? gGlobals->scrWidth : 640;
	const int sh = gGlobals ? gGlobals->scrHeight : 480;
	Menu_DrawText(32, 24, "CS Retro", 255, 174, 0, 255);

	const int itemH = 26;
	int y = sh / 3;
	gHoverMain = -1;
	const bool ingame = InGame();
	int shown = 0;
	for (size_t i = 0; i < gMainItems.size(); i++)
	{
		const GameMenuItem &it = gMainItems[i];
		if (it.onlyInGame && !ingame)
			continue;
		if (it.empty)
		{
			y += itemH / 2;
			continue;
		}
		const int x = 40;
		const bool hover = Menu_Hit(gMouseX, gMouseY, x, y, 360, itemH);
		if (hover)
			gHoverMain = static_cast<int>(i);
		Menu_DrawText(x, y, Menu_L(it.label.c_str()), hover ? 255 : 255, hover ? 220 : 176, hover ? 80 : 0, 255);
		y += itemH;
		shown++;
	}
	(void)shown;
	(void)sw;
}

int UI_VidInit(void)
{
	Menu_LoadLocale();
	gMainItems = Menu_LoadGameMenu();
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
	gBg.clear();
	gMainItems.clear();
	GameUI_HideInGame();
}

void UI_Redraw(float)
{
	if (gInGameOn)
	{
		GameUI_InGameDraw();
		return;
	}
	// V1 UI: PoC oder Options — auch ohne gMenuVisible zeichnen.
	if (VGuiXash_IsUiActive())
	{
		if (gMenuVisible)
			Menu_DrawBackground();
		else if (gEng.pfnFillRGBA && gGlobals)
			gEng.pfnFillRGBA(0, 0, gGlobals->scrWidth, gGlobals->scrHeight, 0, 0, 0, 255);
		VGuiXash_RunFrame();
		VGuiXash_Paint();
		return;
	}
	if (!gMenuVisible)
		return;
	Menu_DrawBackground();
	switch (gScreen)
	{
	case SCREEN_OPTIONS:
		DrawOptions();
		break;
	case SCREEN_NEWGAME:
		DrawNewGame();
		break;
	case SCREEN_BROWSER:
		DrawBrowser();
		break;
	default:
		DrawMain();
		break;
	}
}

void UI_KeyEvent(int key, int down)
{
	if (VGuiXash_IsUiActive())
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
			if (VGuiXash_IsPocActive())
				VGuiXash_HidePocDialog();
			if (VGuiXash_IsOptionsActive())
				VGuiXash_HideOptionsDialog();
			gScreen = SCREEN_MAIN;
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
			gScreen = SCREEN_MAIN;
		else if (InGame())
		{
			gMenuVisible = false;
			gEng.pfnSetKeyDest(KEY_DEST_GAME);
		}
		return;
	}
	if (gScreen == SCREEN_NEWGAME && (key == K_ENTER || key == K_KP_ENTER))
	{
		Profile_Start(&gProfile);
		gScreen = SCREEN_MAIN;
		gMenuVisible = false;
		return;
	}
	if (key == K_MOUSE1 && gScreen == SCREEN_MAIN && gHoverMain >= 0 && gHoverMain < static_cast<int>(gMainItems.size()))
		RunMainCommand(gMainItems[static_cast<size_t>(gHoverMain)].command);
}

void UI_MouseMove(int x, int y)
{
	gMouseX = x;
	gMouseY = y;
	if (VGuiXash_IsUiActive())
		VGuiXash_MouseMove(x, y);
}

void UI_SetActiveMenu(int active)
{
	gMenuVisible = active != 0;
	if (gMenuVisible)
	{
		gScreen = SCREEN_MAIN;
		gEng.pfnSetKeyDest(KEY_DEST_MENU);
		if (gMainItems.empty())
			gMainItems = Menu_LoadGameMenu();
		if (getenv("CSRETRO_V1POC"))
			VGuiXash_ShowPocDialog();
		return;
	}
	// Wie Xash-MainUI UI_CloseMenu: In-Game-VGUI ist Overlay, nicht key_menu.
	if (!gInGameOn)
		gEng.pfnSetKeyDest(KEY_DEST_GAME);
}

void UI_AddServerToList(struct netadr_s, const char *)
{
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
	return gMenuVisible ? 1 : 0;
}

int UI_CreditsActive(void)
{
	return 0;
}

void UI_FinalCredits(void)
{
}
