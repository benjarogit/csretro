#pragma once

const int CSRETRO_LOADOUT_SLOTS = 5;
const int CSRETRO_MAX_PURCHASES = 16;
const int CSRETRO_LOSS_STAGE_MAX = 4;
const int CSRETRO_LOSS_PAYOUT_BASE = 1400;
const int CSRETRO_LOSS_PAYOUT_STEP = 500;

enum BuyItemKind
{
	BUYITEM_WEAPON = 0,
	BUYITEM_VEST,
	BUYITEM_VESTHELM,
	BUYITEM_DEFUSE,
};

struct BuyPurchaseRecord
{
	BuyItemKind kind;
	int weaponId;
	int price;
	int snapshot;
	bool refundable;
};

class CBasePlayer;

void Buy_EnsureLoadout(CBasePlayer *pPlayer);
bool Buy_InLoadout(CBasePlayer *pPlayer, int weaponId);
bool Buy_SetLoadoutFromAliases(CBasePlayer *pPlayer, const char **aliases, int count);
bool Buy_SwapReserve(CBasePlayer *pPlayer, bool rifle, int weaponId);
int Buy_LoadoutWeapon(CBasePlayer *pPlayer, bool rifle, int slot1based);
bool Buy_IsMidTier(int weaponId);
bool Buy_IsRifleSlot(int weaponId);

int Buy_KillReward(int weaponId);
int Buy_LossPayout(TeamName team);
void Buy_OnRoundEnd(int winStatus);
void Buy_OnHalftimeSwap();
void Buy_ResetMatchEconomy();

void Buy_Record(CBasePlayer *pPlayer, BuyItemKind kind, int weaponId, int price);
void Buy_ClearRound(CBasePlayer *pPlayer);
void Buy_OnWeaponFired(CBasePlayer *pPlayer);
void Buy_OnGrenadeThrown(CBasePlayer *pPlayer, int weaponId);
void Buy_RefreshUsedFlags(CBasePlayer *pPlayer);
bool Buy_TryRefundWeapon(CBasePlayer *pPlayer, int weaponId);
bool Buy_TryRefundItem(CBasePlayer *pPlayer, BuyItemKind kind);
int Buy_RefundAll(CBasePlayer *pPlayer);
int Buy_RefundCount(CBasePlayer *pPlayer);

void Buy_NotifyTeammates(CBasePlayer *pPlayer, const char *itemName);
void Buy_SendEco(CBasePlayer *pPlayer);
void Buy_SendEcoAll();
bool Buy_PickupGround(CBasePlayer *pPlayer, int entindex);
void Buy_FillMaxAmmo(CBasePlayer *pPlayer, CBaseEntity *pWeapon);
void Buy_GroundList(CBasePlayer *pPlayer, char *out, int outLen);
