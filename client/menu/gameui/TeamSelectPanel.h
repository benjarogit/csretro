#pragma once

namespace vgui2
{
class Panel;
}

// In-Game Team-Wahl als echte VGUI2-Controls aus Teammenu.res.
// Kein Frame-Dialog: Overlay über der Welt, Scheme ClientScheme (HUD).
bool TeamSelect_Show(vgui2::Panel *root, int validSlots);
void TeamSelect_Hide(bool restoreKeyDest = true);
bool TeamSelect_IsActive();
bool TeamSelect_ActivateSlot(int slot); // 1..10, wie menuselect
void TeamSelect_GateTick();
void TeamSelect_Shutdown();
