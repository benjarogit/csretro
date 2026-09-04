#include "menu_priv.h"
#include "../vgui/vgui_boot.h"

#include <cstring>

ui_enginefuncs_t gEng;
ui_extendedfuncs_t gExtEng;
ui_globalvars_t *gGlobals;
bool gExtEngReady = false;

int UI_VidInit(void);
void UI_Init(void);
void UI_Shutdown(void);
void UI_Redraw(float flTime);
void UI_KeyEvent(int key, int down);
void UI_MouseMove(int x, int y);
void UI_SetActiveMenu(int active);
void UI_AddServerToList(struct netadr_s adr, const char *info);
void UI_GetCursorPos(int *x, int *y);
void UI_SetCursorPos(int x, int y);
void UI_ShowCursor(int show);
void UI_CharEvent(int key);
int UI_MouseInRect(void);
int UI_IsVisible(void);
int UI_CreditsActive(void);
void UI_FinalCredits(void);

static void UI_ConsolePrint(const char *text) { VGuiXash_ConsolePrint(text); }
static void UI_ConsoleClear(void) { VGuiXash_ConsoleClear(); }
static int UI_ConsoleToggle(void) { return VGuiXash_ToggleConsole() ? 1 : 0; }
static int UI_ConsoleIsVisible(void) { return VGuiXash_IsConsoleActive() ? 1 : 0; }

static UI_FUNCTIONS gFunctionTable = {
	UI_VidInit,
	UI_Init,
	UI_Shutdown,
	UI_Redraw,
	UI_KeyEvent,
	UI_MouseMove,
	UI_SetActiveMenu,
	UI_AddServerToList,
	UI_GetCursorPos,
	UI_SetCursorPos,
	UI_ShowCursor,
	UI_CharEvent,
	UI_MouseInRect,
	UI_IsVisible,
	UI_CreditsActive,
	UI_FinalCredits
};

#if defined(_WIN32)
#define CSRETRO_MENU_EXPORT __declspec(dllexport)
#else
#define CSRETRO_MENU_EXPORT __attribute__((visibility("default")))
#endif

extern "C" CSRETRO_MENU_EXPORT int GetMenuAPI(UI_FUNCTIONS *pFunctionTable, ui_enginefuncs_t *engfuncs, ui_globalvars_t *pGlobals)
{
	if (!pFunctionTable || !engfuncs)
		return 0;
	memcpy(pFunctionTable, &gFunctionTable, sizeof(UI_FUNCTIONS));
	memcpy(&gEng, engfuncs, sizeof(ui_enginefuncs_t));
	gGlobals = pGlobals;
	return 1;
}

// Extended Menu API: stops classic Key_Event from forcing SDL text input on every
// menu key. Printable keys then arrive as Xash keynums via UI_KeyEvent → VGuiXash_Key.
extern "C" CSRETRO_MENU_EXPORT int GetExtAPI(int version, UI_EXTENDED_FUNCTIONS *pFunctionTable, ui_extendedfuncs_t *engfuncs)
{
	if (version != MENU_EXTENDED_API_VERSION || !engfuncs)
		return 0;
	memcpy(&gExtEng, engfuncs, sizeof(ui_extendedfuncs_t));
	gExtEngReady = true;
	if (pFunctionTable)
	{
		memset(pFunctionTable, 0, sizeof(UI_EXTENDED_FUNCTIONS));
		pFunctionTable->pfnConsolePrint = UI_ConsolePrint;
		pFunctionTable->pfnConsoleClear = UI_ConsoleClear;
		pFunctionTable->pfnConsoleToggle = UI_ConsoleToggle;
		pFunctionTable->pfnConsoleIsVisible = UI_ConsoleIsVisible;
	}
	return 1;
}
