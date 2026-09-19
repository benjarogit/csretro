#include "precompiled.h"
#include "buy_system.h"

extern int gmsgBuyEco;

namespace
{
const int kTeamBuyNotes = 8;
struct TeamBuyNote
{
	int team;
	char text[48];
};
TeamBuyNote g_teamBuys[kTeamBuyNotes];
int g_nTeamBuys = 0;

const int kTMid[CSRETRO_LOADOUT_SLOTS] = { WEAPON_MAC10, WEAPON_MP5N, WEAPON_UMP45, WEAPON_P90, WEAPON_XM1014 };
const int kCTMid[CSRETRO_LOADOUT_SLOTS] = { WEAPON_TMP, WEAPON_MP5N, WEAPON_UMP45, WEAPON_P90, WEAPON_XM1014 };
const int kTRifle[CSRETRO_LOADOUT_SLOTS] = { WEAPON_GALIL, WEAPON_AK47, WEAPON_SCOUT, WEAPON_AWP, WEAPON_G3SG1 };
const int kCTRifle[CSRETRO_LOADOUT_SLOTS] = { WEAPON_FAMAS, WEAPON_M4A1, WEAPON_SCOUT, WEAPON_AWP, WEAPON_SG550 };

const int kMidPool[] = {
	WEAPON_MAC10, WEAPON_TMP, WEAPON_MP5N, WEAPON_UMP45, WEAPON_P90, WEAPON_M3, WEAPON_XM1014, WEAPON_M249
};
const int kRiflePoolT[] = {
	WEAPON_GALIL, WEAPON_AK47, WEAPON_SG552, WEAPON_SCOUT, WEAPON_AWP, WEAPON_G3SG1
};
const int kRiflePoolCT[] = {
	WEAPON_FAMAS, WEAPON_M4A1, WEAPON_AUG, WEAPON_SCOUT, WEAPON_AWP, WEAPON_SG550
};

CCSPlayer *CS(CBasePlayer *p)
{
	return p ? p->CSPlayer() : nullptr;
}

int SnapshotWeapon(CBasePlayer *p, int weaponId)
{
	auto *cs = CS(p);
	if (!cs)
		return 0;
	CBasePlayerItem *item = cs->GetItemById(static_cast<WeaponIdType>(weaponId));
	auto *wpn = item ? static_cast<CBasePlayerWeapon *>(item->GetWeaponPtr()) : nullptr;
	if (!wpn)
		return 0;
	int ammo = wpn->m_iClip;
	if (wpn->m_iPrimaryAmmoType >= 0)
		ammo += p->m_rgAmmo[wpn->m_iPrimaryAmmoType];
	return ammo;
}

int SnapshotArmor(CBasePlayer *p)
{
	int v = static_cast<int>(p->pev->armorvalue);
	if (p->m_iKevlar == ARMOR_VESTHELM)
		v += 1000;
	return v;
}

void CopyDefaults(CBasePlayer *p)
{
	auto *cs = CS(p);
	if (!cs)
		return;
	const bool ct = p->m_iTeam == CT;
	const int *mid = ct ? kCTMid : kTMid;
	const int *rifle = ct ? kCTRifle : kTRifle;
	for (int i = 0; i < CSRETRO_LOADOUT_SLOTS; ++i)
	{
		cs->m_loadoutMid[i] = mid[i];
		cs->m_loadoutRifle[i] = rifle[i];
	}
	cs->m_bLoadoutReady = true;
}

bool InArray(const int *ids, int n, int weaponId)
{
	for (int i = 0; i < n; ++i)
	{
		if (ids[i] == weaponId)
			return true;
	}
	return false;
}

bool TeamAllows(CBasePlayer *p, int weaponId)
{
	if (p->m_iTeam == CT)
	{
		if (weaponId == WEAPON_MAC10 || weaponId == WEAPON_GALIL || weaponId == WEAPON_AK47
			|| weaponId == WEAPON_SG552 || weaponId == WEAPON_G3SG1 || weaponId == WEAPON_ELITE
			|| weaponId == WEAPON_MOLOTOV)
			return false;
	}
	else
	{
		if (weaponId == WEAPON_TMP || weaponId == WEAPON_FAMAS || weaponId == WEAPON_M4A1
			|| weaponId == WEAPON_AUG || weaponId == WEAPON_SG550 || weaponId == WEAPON_FIVESEVEN
			|| weaponId == WEAPON_INCGRENADE)
			return false;
	}
	return true;
}

const char *ItemLabel(BuyItemKind kind, int weaponId)
{
	if (kind == BUYITEM_VEST)
		return "Kevlar";
	if (kind == BUYITEM_VESTHELM)
		return "Kevlar+Helm";
	if (kind == BUYITEM_DEFUSE)
		return "Defuse Kit";
	const char *alias = WeaponIDToAlias(weaponId);
	return alias ? alias : "item";
}

void RemovePurchaseAt(CCSPlayer *cs, int index)
{
	if (!cs || index < 0 || index >= cs->m_nPurchases)
		return;
	for (int i = index; i < cs->m_nPurchases - 1; ++i)
		cs->m_purchases[i] = cs->m_purchases[i + 1];
	cs->m_nPurchases--;
}

bool StripWeapon(CBasePlayer *p, int weaponId)
{
	WeaponInfoStruct *info = GetWeaponInfo(weaponId);
	if (!p || !info || !info->entityName || !p->CSPlayer())
		return false;
	if (weaponId == WEAPON_FLASHBANG || weaponId == WEAPON_HEGRENADE || weaponId == WEAPON_SMOKEGRENADE
		|| weaponId == WEAPON_MOLOTOV || weaponId == WEAPON_INCGRENADE)
	{
		int ammo = p->AmmoInventory(info->ammoType);
		if (ammo > 1)
		{
			p->m_rgAmmo[info->ammoType] = ammo - 1;
			return true;
		}
	}
	p->CSPlayer()->RemovePlayerItemEx(info->entityName, true);
	return true;
}
} // namespace

bool Buy_IsMidTier(int weaponId)
{
	return InArray(kMidPool, static_cast<int>(sizeof(kMidPool) / sizeof(kMidPool[0])), weaponId);
}

bool Buy_IsRifleSlot(int weaponId)
{
	return InArray(kRiflePoolT, static_cast<int>(sizeof(kRiflePoolT) / sizeof(kRiflePoolT[0])), weaponId)
		|| InArray(kRiflePoolCT, static_cast<int>(sizeof(kRiflePoolCT) / sizeof(kRiflePoolCT[0])), weaponId);
}

void Buy_EnsureLoadout(CBasePlayer *pPlayer)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return;
	if (!cs->m_bLoadoutReady)
	{
		CopyDefaults(pPlayer);
		return;
	}
	for (int i = 0; i < CSRETRO_LOADOUT_SLOTS; ++i)
	{
		if (!Buy_IsMidTier(cs->m_loadoutMid[i]) || !TeamAllows(pPlayer, cs->m_loadoutMid[i])
			|| !Buy_IsRifleSlot(cs->m_loadoutRifle[i]) || !TeamAllows(pPlayer, cs->m_loadoutRifle[i]))
		{
			CopyDefaults(pPlayer);
			return;
		}
	}
}

bool Buy_InLoadout(CBasePlayer *pPlayer, int weaponId)
{
	if (!Buy_IsMidTier(weaponId) && !Buy_IsRifleSlot(weaponId))
		return true;
	Buy_EnsureLoadout(pPlayer);
	auto *cs = CS(pPlayer);
	if (!cs)
		return false;
	if (InArray(cs->m_loadoutMid, CSRETRO_LOADOUT_SLOTS, weaponId))
		return true;
	if (InArray(cs->m_loadoutRifle, CSRETRO_LOADOUT_SLOTS, weaponId))
		return true;
	return false;
}

bool Buy_SetLoadoutFromAliases(CBasePlayer *pPlayer, const char **aliases, int count)
{
	auto *cs = CS(pPlayer);
	if (!cs || count < CSRETRO_LOADOUT_SLOTS * 2)
		return false;
	int mid[CSRETRO_LOADOUT_SLOTS];
	int rifle[CSRETRO_LOADOUT_SLOTS];
	for (int i = 0; i < CSRETRO_LOADOUT_SLOTS; ++i)
	{
		mid[i] = AliasToWeaponID(aliases[i]);
		rifle[i] = AliasToWeaponID(aliases[i + CSRETRO_LOADOUT_SLOTS]);
		if (!Buy_IsMidTier(mid[i]) || !TeamAllows(pPlayer, mid[i]))
			return false;
		if (!Buy_IsRifleSlot(rifle[i]) || !TeamAllows(pPlayer, rifle[i]))
			return false;
	}
	for (int i = 0; i < CSRETRO_LOADOUT_SLOTS; ++i)
	{
		for (int j = i + 1; j < CSRETRO_LOADOUT_SLOTS; ++j)
		{
			if (mid[i] == mid[j] || rifle[i] == rifle[j])
				return false;
		}
	}
	for (int i = 0; i < CSRETRO_LOADOUT_SLOTS; ++i)
	{
		cs->m_loadoutMid[i] = mid[i];
		cs->m_loadoutRifle[i] = rifle[i];
	}
	cs->m_bLoadoutReady = true;
	return true;
}

bool Buy_SwapReserve(CBasePlayer *pPlayer, bool rifle, int weaponId)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return false;
	Buy_EnsureLoadout(pPlayer);
	if (!TeamAllows(pPlayer, weaponId))
		return false;
	int *slots = rifle ? cs->m_loadoutRifle : cs->m_loadoutMid;
	if (rifle ? !Buy_IsRifleSlot(weaponId) : !Buy_IsMidTier(weaponId))
		return false;
	if (InArray(slots, CSRETRO_LOADOUT_SLOTS, weaponId))
		return true;
	slots[CSRETRO_LOADOUT_SLOTS - 1] = weaponId;
	return true;
}

int Buy_LoadoutWeapon(CBasePlayer *pPlayer, bool rifle, int slot1based)
{
	Buy_EnsureLoadout(pPlayer);
	auto *cs = CS(pPlayer);
	if (!cs || slot1based < 1 || slot1based > CSRETRO_LOADOUT_SLOTS)
		return WEAPON_NONE;
	return rifle ? cs->m_loadoutRifle[slot1based - 1] : cs->m_loadoutMid[slot1based - 1];
}

int Buy_KillReward(int weaponId)
{
	return GetWeaponKillReward(weaponId);
}

int Buy_LossPayout(TeamName team)
{
	if (!CSGameRules())
		return CSRETRO_LOSS_PAYOUT_BASE;
	int stage = (team == CT) ? CSGameRules()->m_iCTLossStage : CSGameRules()->m_iTerroristLossStage;
	if (stage < 0)
		stage = 0;
	if (stage > CSRETRO_LOSS_STAGE_MAX)
		stage = CSRETRO_LOSS_STAGE_MAX;
	return CSRETRO_LOSS_PAYOUT_BASE + stage * CSRETRO_LOSS_PAYOUT_STEP;
}

void Buy_OnRoundEnd(int winStatus)
{
	if (!CSGameRules())
		return;
	auto *g = CSGameRules();
	if (winStatus == WINSTATUS_TERRORISTS)
	{
		g->m_iLoserBonus = Buy_LossPayout(CT);
		g->m_iCTLossStage = Q_min(CSRETRO_LOSS_STAGE_MAX, g->m_iCTLossStage + 1);
		g->m_iTerroristLossStage = Q_max(0, g->m_iTerroristLossStage - 1);
	}
	else if (winStatus == WINSTATUS_CTS)
	{
		g->m_iLoserBonus = Buy_LossPayout(TERRORIST);
		g->m_iTerroristLossStage = Q_min(CSRETRO_LOSS_STAGE_MAX, g->m_iTerroristLossStage + 1);
		g->m_iCTLossStage = Q_max(0, g->m_iCTLossStage - 1);
	}
	g_nTeamBuys = 0;
}

void Buy_OnHalftimeSwap()
{
	if (!CSGameRules())
		return;
	auto *g = CSGameRules();
	g->m_iCTLossStage = Q_min(CSRETRO_LOSS_STAGE_MAX, g->m_iCTLossStage + 1);
	g->m_iTerroristLossStage = Q_min(CSRETRO_LOSS_STAGE_MAX, g->m_iTerroristLossStage + 1);
}

void Buy_ResetMatchEconomy()
{
	if (!CSGameRules())
		return;
	CSGameRules()->m_iCTLossStage = 0;
	CSGameRules()->m_iTerroristLossStage = 0;
	CSGameRules()->m_iLoserBonus = CSRETRO_LOSS_PAYOUT_BASE;
	g_nTeamBuys = 0;
}

void Buy_Record(CBasePlayer *pPlayer, BuyItemKind kind, int weaponId, int price)
{
	auto *cs = CS(pPlayer);
	if (!cs || price <= 0)
		return;
	if (cs->m_nPurchases >= CSRETRO_MAX_PURCHASES)
		return;
	CSBuyPurchase rec;
	rec.kind = kind;
	rec.weaponId = weaponId;
	rec.price = price;
	rec.refundable = true;
	if (kind == BUYITEM_WEAPON)
		rec.snapshot = SnapshotWeapon(pPlayer, weaponId);
	else
		rec.snapshot = SnapshotArmor(pPlayer);
	cs->m_purchases[cs->m_nPurchases++] = rec;
	Buy_NotifyTeammates(pPlayer, ItemLabel(kind, weaponId));
	Buy_SendEco(pPlayer);
}

void Buy_ClearRound(CBasePlayer *pPlayer)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return;
	cs->m_nPurchases = 0;
}

void Buy_RefreshUsedFlags(CBasePlayer *pPlayer)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return;
	for (int i = 0; i < cs->m_nPurchases; ++i)
	{
		CSBuyPurchase &rec = cs->m_purchases[i];
		if (!rec.refundable)
			continue;
		if (rec.kind == BUYITEM_WEAPON)
		{
			if (SnapshotWeapon(pPlayer, rec.weaponId) < rec.snapshot)
				rec.refundable = false;
		}
		else if (SnapshotArmor(pPlayer) < rec.snapshot)
			rec.refundable = false;
	}
}

void Buy_OnWeaponFired(CBasePlayer *pPlayer)
{
	if (!pPlayer || !pPlayer->m_pActiveItem)
		return;
	Buy_OnGrenadeThrown(pPlayer, pPlayer->m_pActiveItem->m_iId);
}

void Buy_OnGrenadeThrown(CBasePlayer *pPlayer, int weaponId)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return;
	for (int i = 0; i < cs->m_nPurchases; ++i)
	{
		if (cs->m_purchases[i].kind == BUYITEM_WEAPON && cs->m_purchases[i].weaponId == weaponId)
			cs->m_purchases[i].refundable = false;
	}
}

bool Buy_TryRefundWeapon(CBasePlayer *pPlayer, int weaponId)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return false;
	Buy_RefreshUsedFlags(pPlayer);
	for (int i = cs->m_nPurchases - 1; i >= 0; --i)
	{
		CSBuyPurchase &rec = cs->m_purchases[i];
		if (rec.kind != BUYITEM_WEAPON || rec.weaponId != weaponId || !rec.refundable)
			continue;
		if (!StripWeapon(pPlayer, weaponId))
			return false;
		pPlayer->AddAccount(rec.price, RT_PLAYER_BOUGHT_SOMETHING);
		RemovePurchaseAt(cs, i);
		Buy_SendEco(pPlayer);
		pPlayer->BuildRebuyStruct(true);
		return true;
	}
	return false;
}

bool Buy_TryRefundItem(CBasePlayer *pPlayer, BuyItemKind kind)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return false;
	Buy_RefreshUsedFlags(pPlayer);
	for (int i = cs->m_nPurchases - 1; i >= 0; --i)
	{
		CSBuyPurchase &rec = cs->m_purchases[i];
		if (rec.kind != kind || !rec.refundable)
			continue;
		if (kind == BUYITEM_DEFUSE)
		{
			if (!pPlayer->m_bHasDefuser)
				return false;
			pPlayer->RemoveDefuser();
		}
		else
		{
			pPlayer->pev->armorvalue = 0;
			pPlayer->m_iKevlar = ARMOR_NONE;
			pPlayer->SetScoreboardAttributes();
		}
		pPlayer->AddAccount(rec.price, RT_PLAYER_BOUGHT_SOMETHING);
		RemovePurchaseAt(cs, i);
		Buy_SendEco(pPlayer);
		pPlayer->BuildRebuyStruct(true);
		return true;
	}
	return false;
}

int Buy_RefundAll(CBasePlayer *pPlayer)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return 0;
	Buy_RefreshUsedFlags(pPlayer);
	int n = 0;
	for (int i = cs->m_nPurchases - 1; i >= 0; --i)
	{
		CSBuyPurchase rec = cs->m_purchases[i];
		if (!rec.refundable)
			continue;
		if (rec.kind == BUYITEM_WEAPON)
		{
			if (Buy_TryRefundWeapon(pPlayer, rec.weaponId))
				++n;
		}
		else if (Buy_TryRefundItem(pPlayer, static_cast<BuyItemKind>(rec.kind)))
			++n;
	}
	return n;
}

int Buy_RefundCount(CBasePlayer *pPlayer)
{
	auto *cs = CS(pPlayer);
	if (!cs)
		return 0;
	Buy_RefreshUsedFlags(pPlayer);
	int n = 0;
	for (int i = 0; i < cs->m_nPurchases; ++i)
	{
		if (cs->m_purchases[i].refundable)
			++n;
	}
	return n;
}

void Buy_NotifyTeammates(CBasePlayer *pPlayer, const char *itemName)
{
	if (!pPlayer || !itemName || !itemName[0])
		return;
	if (g_nTeamBuys >= kTeamBuyNotes)
	{
		for (int i = 1; i < kTeamBuyNotes; ++i)
			g_teamBuys[i - 1] = g_teamBuys[i];
		g_nTeamBuys = kTeamBuyNotes - 1;
	}
	TeamBuyNote &note = g_teamBuys[g_nTeamBuys++];
	note.team = pPlayer->m_iTeam;
	const char *name = STRING(pPlayer->pev->netname);
	Q_snprintf(note.text, sizeof(note.text), "%s %s", (name && name[0]) ? name : "Player", itemName);
	Buy_SendEcoAll();
}

void Buy_GroundList(CBasePlayer *pPlayer, char *out, int outLen)
{
	if (!out || outLen <= 0)
		return;
	out[0] = '\0';
	if (!pPlayer)
		return;
	CBaseEntity *ent = nullptr;
	int n = 0;
	char buf[80] = {};
	while ((ent = UTIL_FindEntityByClassname(ent, "weaponbox")))
	{
		if ((ent->pev->origin - pPlayer->pev->origin).Length() > 512.0f)
			continue;
		auto *box = static_cast<CWeaponBox *>(ent);
		for (int slot = 0; slot < MAX_ITEM_TYPES; ++slot)
		{
			for (CBasePlayerItem *item = box->m_rgpPlayerItems[slot]; item; item = item->m_pNext)
			{
				const char *alias = WeaponIDToAlias(item->m_iId);
				if (!alias)
					continue;
				if (n)
					Q_strlcat(buf, ",", sizeof(buf));
				char piece[24];
				Q_snprintf(piece, sizeof(piece), "%d:%s", ENTINDEX(ent->edict()), alias);
				Q_strlcat(buf, piece, sizeof(buf));
				++n;
				if (n >= 4)
					break;
			}
			if (n >= 4)
				break;
		}
		if (n >= 4)
			break;
	}
	Q_strlcpy(out, buf, outLen);
}

void Buy_SendEco(CBasePlayer *pPlayer)
{
	if (!pPlayer || !gmsgBuyEco)
		return;
	char mates[160] = {};
	for (int i = 0; i < g_nTeamBuys; ++i)
	{
		if (g_teamBuys[i].team != pPlayer->m_iTeam)
			continue;
		if (mates[0])
			Q_strlcat(mates, " | ", sizeof(mates));
		Q_strlcat(mates, g_teamBuys[i].text, sizeof(mates));
	}
	char ground[80] = {};
	Buy_GroundList(pPlayer, ground, sizeof(ground));
	const int loss = Buy_LossPayout(pPlayer->m_iTeam);
	int nextMin = pPlayer->m_iAccount + loss;
	if (nextMin > static_cast<int>(maxmoney.value))
		nextMin = static_cast<int>(maxmoney.value);
	MESSAGE_BEGIN(MSG_ONE, gmsgBuyEco, nullptr, pPlayer->pev);
		WRITE_SHORT(loss);
		WRITE_SHORT(nextMin);
		WRITE_BYTE(Buy_RefundCount(pPlayer));
		WRITE_STRING(mates);
		WRITE_STRING(ground);
	MESSAGE_END();
}

void Buy_SendEcoAll()
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer *p = UTIL_PlayerByIndex(i);
		if (p && p->IsPlayer() && !p->IsDormant())
			Buy_SendEco(p);
	}
}

bool Buy_PickupGround(CBasePlayer *pPlayer, int entindex)
{
	if (!pPlayer || !pPlayer->CanPlayerBuy(true))
		return false;
	edict_t *pent = INDEXENT(entindex);
	if (FNullEnt(pent))
		return false;
	CBaseEntity *ent = CBaseEntity::Instance(pent);
	if (!ent || !FClassnameIs(ent->pev, "weaponbox"))
		return false;
	if ((ent->pev->origin - pPlayer->pev->origin).Length() > 512.0f)
		return false;
	ent->Touch(pPlayer);
	Buy_SendEco(pPlayer);
	return true;
}

void Buy_FillMaxAmmo(CBasePlayer *pPlayer, CBaseEntity *pWeapon)
{
	if (!pPlayer || !pWeapon)
		return;
	auto *item = static_cast<CBasePlayerItem *>(pWeapon);
	if (!item->pszAmmo1() || item->iMaxAmmo1() <= 0)
		return;
	pPlayer->GiveAmmo(item->iMaxAmmo1(), item->pszAmmo1(), item->iMaxAmmo1());
}
