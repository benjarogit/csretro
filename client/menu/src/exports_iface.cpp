#include "menu_priv.h"
#include "../vgui/vgui_boot.h"
#include "../gameui/SpectatorHudPanel.h"
#include "../gameui/ScoreboardHudPanel.h"
#include "../gameui/BuySelectPanel.h"

#include "interface.h"
#include "cl_dll/IGameMenuExports.h"

#include <cstring>

class CGameMenuExports : public IGameMenuExports
{
public:
	bool Initialize(CreateInterfaceFn) override { return true; }
	const char *L(const char *szStr) override { return Menu_L(szStr); }
	bool IsActive(void) override { return GameUI_InGameActive() || gMenuVisible || VGuiXash_IsConsoleActive(); }
	bool IsMainMenuActive(void) override { return gMenuVisible && !GameUI_InGameActive(); }
	bool IsModalInGame(void) override
	{
		return VGuiXash_IsTeamSelectActive() || VGuiXash_IsClassSelectActive() ||
			VGuiXash_IsBuySelectActive();
	}
	void Key(int key, int down) override { GameUI_InGameKey(key, down); }
	void MouseMove(int x, int y) override
	{
		gMouseX = x;
		gMouseY = y;
	}
	HFont BuildFont(CFontBuilder &) override { return 0; }
	void GetCharABCWide(HFont, int, int &a, int &b, int &c) override { a = 0; b = 8; c = 0; }
	int GetFontTall(HFont) override { return 16; }
	int GetCharacterWidth(HFont, int, int) override { return 8; }
	void GetTextSize(HFont, const char *text, int *wide, int *height, int) override
	{
		if (wide)
			*wide = text ? static_cast<int>(strlen(text)) * 8 : 0;
		if (height)
			*height = 16;
	}
	int GetTextHeight(HFont, const char *, int) override { return 16; }
	int DrawCharacter(HFont, int, int, int, int, const unsigned int, bool) override { return 8; }
	void SetupScoreboard(int, int, int, int, unsigned int, bool) override {}
	void DrawScoreboard(void) override {}
	void DrawSpectatorMenu(void) override {}
	void SetSpectatorHud(const SpectatorHudState *state) override { SpectatorHud_Set(state); }
	void SetScoreboardHud(const ScoreboardHudState *state) override { ScoreboardHud_Set(state); }
	void SetBuyHud(const BuyHudState *state) override { BuySelect_SetHud(state); }
	void CloseDeveloperConsole(void) override { VGuiXash_HideConsole(); }
	void ShowVGUIMenu(int menuType, int param1, int param2) override
	{
		Menu_NotePlayerTeam(param2);
		GameUI_ShowInGame(menuType, param1);
	}
	void HideVGUIMenu(void) override { GameUI_HideInGame(); }
};

static CGameMenuExports gMenuExports;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMenuExports, IGameMenuExports, GAMEMENUEXPORTS_INTERFACE_VERSION, gMenuExports);

#if defined(_WIN32)
#define CSRETRO_MENU_EXPORT __declspec(dllexport)
#else
#define CSRETRO_MENU_EXPORT __attribute__((visibility("default")))
#endif

// Direkter Getter — unabhängig von CreateInterface/InterfaceReg-Interposition
// zwischen Client- und Menü-Lib (beide linken interface.cpp).
extern "C" CSRETRO_MENU_EXPORT IGameMenuExports *Csretro_GetGameMenuExports(void)
{
	return &gMenuExports;
}
