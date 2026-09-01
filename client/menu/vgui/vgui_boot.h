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
bool VGuiXash_IsUiActive(); // PoC oder Options
void VGuiXash_Key(int key, int down);
void VGuiXash_MouseMove(int x, int y);
void VGuiXash_Char(int ch);

// Test injection into IInputInternal (optional; xdotool path is primary).
void PocDialog_InjectMouse(int x, int y, bool leftDown, bool leftUp);
void PocDialog_InjectKey(int vguiKeyCode, bool down);
void PocDialog_InjectChar(wchar_t ch);
