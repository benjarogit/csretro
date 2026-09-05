#pragma once

#include <cwchar>

void VGuiXash_Init();
void VGuiXash_Shutdown();
void VGuiXash_RunFrame();
void VGuiXash_Paint();
bool VGuiXash_ShowPocDialog();
void VGuiXash_HidePocDialog();
bool VGuiXash_IsPocActive();
bool VGuiXash_ShowOptionsDialog();
void VGuiXash_HideOptionsDialog();
bool VGuiXash_IsOptionsActive();
bool VGuiXash_IsVideoCalibrationActive();
bool VGuiXash_ShowMainMenu();
void VGuiXash_HideMainMenu();
bool VGuiXash_IsMainMenuActive();
bool VGuiXash_ShowCreateGameDialog();
void VGuiXash_HideCreateGameDialog();
bool VGuiXash_IsCreateGameActive();
bool VGuiXash_ShowServerBrowser();
void VGuiXash_HideServerBrowser();
bool VGuiXash_IsServerBrowserActive();
bool VGuiXash_ToggleConsole();
void VGuiXash_HideConsole();
bool VGuiXash_IsConsoleActive();
void VGuiXash_ConsolePrint(const char *text);
void VGuiXash_ConsoleClear();

class CCreateGameDialog;
class COptionsDialog;
COptionsDialog *VGuiXash_GateGetOptionsDialog();
CCreateGameDialog *VGuiXash_GateGetCreateGameDialog();
bool VGuiXash_IsKeyboardCapturing();
bool VGuiXash_ShowTeamSelect(int validSlots);
void VGuiXash_HideTeamSelect();
bool VGuiXash_IsTeamSelectActive();
bool VGuiXash_TeamActivateSlot(int slot);
bool VGuiXash_ShowClassSelect(int menuType, int validSlots);
void VGuiXash_HideClassSelect();
bool VGuiXash_IsClassSelectActive();
bool VGuiXash_ClassActivateSlot(int slot);
bool VGuiXash_ShowBuySelect(int menuType, int validSlots);
void VGuiXash_HideBuySelect();
bool VGuiXash_IsBuySelectActive();
bool VGuiXash_BuyActivateSlot(int slot);
bool VGuiXash_ShowRadioSelect(int menuType, int validSlots);
void VGuiXash_HideRadioSelect();
bool VGuiXash_IsRadioSelectActive();
bool VGuiXash_RadioActivateSlot(int slot);
void VGuiXash_HideSpectatorHud();
bool VGuiXash_IsSpectatorActive();
void VGuiXash_HideScoreboardHud();
bool VGuiXash_IsScoreboardActive();
bool VGuiXash_IsInteractiveUiActive(); // alles außer Spectator-/Scoreboard-HUD
bool VGuiXash_IsUiActive(); // Interactive + Spectator/Scoreboard-Paint
void VGuiXash_Key(int key, int down);
void VGuiXash_MouseMove(int x, int y);
void VGuiXash_Char(int ch);

// Test injection into IInputInternal (optional; xdotool path is primary).
void PocDialog_InjectMouse(int x, int y, bool leftDown, bool leftUp);
void PocDialog_InjectKey(int vguiKeyCode, bool down);
void PocDialog_InjectChar(wchar_t ch);
