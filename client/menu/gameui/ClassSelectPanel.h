#pragma once

namespace vgui2
{
class Panel;
}

// In-Game Class-Wahl als echte VGUI2-Controls aus Classmenu_TER/CT.res.
// Kein Frame-Dialog: Overlay über der Welt, Scheme ClientScheme (HUD).
bool ClassSelect_Show(vgui2::Panel *root, int menuType, int validSlots);
void ClassSelect_Hide(bool restoreKeyDest = true);
bool ClassSelect_IsActive();
bool ClassSelect_ActivateSlot(int slot); // 1..10, wie menuselect
int ClassSelect_MenuType();
void ClassSelect_GateTick();
void ClassSelect_Shutdown();
