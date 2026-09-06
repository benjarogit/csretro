#include "InGameRoster.h"

#include <cstring>

namespace
{
ScoreboardHudState g_roster{};
}

void InGameRoster_StoreScoreboard(const ScoreboardHudState &state)
{
	g_roster.visible = state.visible;
	g_roster.tScore = state.tScore;
	g_roster.ctScore = state.ctScore;
	g_roster.playerCount = state.playerCount;
	std::memcpy(g_roster.server, state.server, sizeof(g_roster.server));
	std::memcpy(g_roster.players, state.players, sizeof(g_roster.players));
}

void InGameRoster_StoreSpectator(const SpectatorHudState &state)
{
	if (state.playerCount <= 0)
		return;
	g_roster.tScore = state.tScore;
	g_roster.ctScore = state.ctScore;
	g_roster.playerCount = state.playerCount;
	std::memcpy(g_roster.players, state.players, sizeof(g_roster.players));
}

const ScoreboardHudState &InGameRoster_Get()
{
	return g_roster;
}
