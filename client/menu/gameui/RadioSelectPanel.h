#pragma once

namespace vgui2
{
class Panel;
}

// In-Game Radio: eigene Familie, nicht Team/Class/Buy-Viewport und nicht GameUI-Frame.
// Kompakte HUD-Liste (CS 1.6 titles.txt + CS:GO RadioPanel: raised group, kein Dim).
// Commands = ReGameDLL-Aliase. ShowMenu bleibt Legacy.
bool RadioSelect_Show(vgui2::Panel *root, int menuType, int validSlots);
void RadioSelect_Hide();
bool RadioSelect_IsActive();
bool RadioSelect_ActivateSlot(int slot); // 1..10, wie menuselect
int RadioSelect_MenuType();
void RadioSelect_GateTick();
void RadioSelect_Shutdown();
