#pragma once

struct SpectatorHudState;

namespace vgui2
{
class Panel;
}

// Spectator: eigene Familie. Broadcast-Rahmen, Daten am Rand, Welt sichtbar.
// Kein Team-Viewport, kein KEY_DEST_MENU. Steam Spectator.res = Felder, nicht Overlay.
void SpectatorHud_BindHost(vgui2::Panel *root);
void SpectatorHud_Set(const SpectatorHudState *state);
void SpectatorHud_Hide();
bool SpectatorHud_IsActive();
void SpectatorHud_GateTick();
void SpectatorHud_Shutdown();
