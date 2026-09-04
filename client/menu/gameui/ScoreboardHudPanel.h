#pragma once

struct ScoreboardHudState;

namespace vgui2
{
class Panel;
}

// Scoreboard: eigene Familie. Mittige Tafel mit T/CT-Akzent, TAB hält, Welt sichtbar.
// Kein Team-Viewport, kein Spectator-Klon, kein KEY_DEST_MENU.
void ScoreboardHud_BindHost(vgui2::Panel *root);
void ScoreboardHud_Set(const ScoreboardHudState *state);
void ScoreboardHud_Hide();
bool ScoreboardHud_IsActive();
void ScoreboardHud_GateTick();
void ScoreboardHud_Shutdown();
