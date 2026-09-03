#pragma once

#include "xash3d_types.h"
#include "menu_int.h"

#include <cstddef>
#include <string>
#include <vector>

struct ResField
{
	std::string name;
	std::string control;
	std::string label;
	std::string command;
	int x = 0, y = 0, w = 0, h = 0;
	bool visible = true;
	bool enabled = true;
};

struct GameMenuItem
{
	std::string label;
	std::string command;
	bool onlyInGame = false;
	bool empty = false;
};

struct ServerProfile
{
	std::string map = "de_dust";
	std::string hostname = "CS Retro";
	std::string password;
	int maxplayers = 10;
	int lan = 1;
	float roundtime = 5.f;
	float freezetime = 6.f;
	int friendlyfire = 0;
	int teambalance = 1;
	int bot_quota = 0;
	int bot_difficulty = 0;
	std::string bot_join_team = "any";
	std::string modules = "none";
};

extern ui_enginefuncs_t gEng;
extern ui_extendedfuncs_t gExtEng;
extern ui_globalvars_t *gGlobals;
extern bool gExtEngReady;

void Menu_Con(const char *fmt, ...);
const char *Menu_L(const char *token);
void Menu_LoadLocale();
std::vector<ResField> Menu_LoadRes(const char *path);
std::vector<GameMenuItem> Menu_LoadGameMenu();
void Menu_LoadBackground();
void Menu_DrawBackground();
void Menu_DrawText(int x, int y, const char *text, int r, int g, int b, int a);
bool Menu_Hit(int mx, int my, int x, int y, int w, int h);

void Profile_Defaults(ServerProfile *p);
void Profile_WriteListen(const ServerProfile *p);
void Profile_Start(const ServerProfile *p);

void GameUI_ShowInGame(int menuType);
void GameUI_HideInGame();
bool GameUI_InGameActive();
bool GameUI_ActivateSlot(int slot); // 1..10
void GameUI_InGameKey(int key, int down);
void GameUI_InGameDraw();

void GameUI_OpenOptions();
void GameUI_OpenNewGame();
void GameUI_OpenBrowser();

enum MenuScreen
{
	SCREEN_MAIN = 0,
	SCREEN_OPTIONS,
	SCREEN_NEWGAME,
	SCREEN_BROWSER
};

extern MenuScreen gScreen;
extern bool gMenuVisible;
extern int gMouseX, gMouseY;
extern ServerProfile gProfile;
