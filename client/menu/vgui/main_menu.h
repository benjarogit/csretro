#pragma once

namespace vgui2
{
class Panel;
}

// Classic CS-1.6 main menu as real VGUI2 controls (BasePanel + Menu + MenuItems).
// Replaces the interim console-string menu; entries come from resource/GameMenu.res.
bool MainMenu_Show(vgui2::Panel *root);
void MainMenu_Hide();
bool MainMenu_IsActive();

// Re-evaluates OnlyInGame / notsingle visibility after a level change.
void MainMenu_UpdateItemState();

// Screen size changed — re-anchor the menu.
void MainMenu_InvalidateLayout();

// Per-frame hook; only active under CSRETRO_MAINMENU_GATE (delayed engine screenshot).
void MainMenu_GateTick();

// Per-frame hook; only active under CSRETRO_PAUSE_GATE (in-game Escape → Pause).
void MainMenu_PauseGateTick();

void MainMenu_Shutdown();
