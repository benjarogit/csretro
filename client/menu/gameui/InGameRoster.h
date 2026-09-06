#pragma once

#include "cl_dll/IGameMenuExports.h"

// Letzter echter Spielerstand aus Scoreboard- oder Spectator-Push.
// Team/Class lesen denselben Cache — kein zweites Player-Protokoll.
void InGameRoster_StoreScoreboard(const ScoreboardHudState &state);
void InGameRoster_StoreSpectator(const SpectatorHudState &state);
const ScoreboardHudState &InGameRoster_Get();
