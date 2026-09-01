#include "menu_priv.h"

#include "interface.h"
#include "cl_dll/IGameMenuExports.h"

#include <cstring>

class CGameMenuExports : public IGameMenuExports
{
public:
	bool Initialize(CreateInterfaceFn) override { return true; }
	const char *L(const char *szStr) override { return Menu_L(szStr); }
	bool IsActive(void) override { return GameUI_InGameActive() || gMenuVisible; }
	bool IsMainMenuActive(void) override { return gMenuVisible && !GameUI_InGameActive(); }
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
	void ShowVGUIMenu(int menuType, int, int) override { GameUI_ShowInGame(menuType); }
	void HideVGUIMenu(void) override { GameUI_HideInGame(); }
};

static CGameMenuExports gMenuExports;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMenuExports, IGameMenuExports, GAMEMENUEXPORTS_INTERFACE_VERSION, gMenuExports);
