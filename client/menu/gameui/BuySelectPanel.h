#pragma once

struct BuyHudState;

namespace vgui2
{
class Panel;
}

// In-Game Buy als echte VGUI2-Controls aus Steam Buy*.res.
// Overlay über der Welt, Scheme ClientScheme (HUD). Kein Frame-Dialog.
bool BuySelect_Show(vgui2::Panel *root, int menuType, int validSlots);
void BuySelect_Hide();
bool BuySelect_IsActive();
bool BuySelect_ActivateSlot(int slot); // 1..10, wie menuselect
int BuySelect_MenuType();
void BuySelect_GateTick();
void BuySelect_AfterFrame();
void BuySelect_SetHud(const BuyHudState *state);
void BuySelect_Shutdown();
