#include "CreateGameServerPage.h"
#include "CreateGameSettingsList.h"

#include <FileSystem.h>
#include <tier1/KeyValues.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RadioButton.h>

#include "../src/menu_priv.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace vgui2;

namespace
{
const char *const kSkillFields[4] = {
	"SkillLevelEasy", "SkillLevelNormal", "SkillLevelHard", "SkillLevelExpert"};

int ClampInt(int value, int lo, int hi)
{
	return std::max(lo, std::min(hi, value));
}
} // namespace

CCreateGameServerPage::CCreateGameServerPage(Panel *parent)
	: BaseClass(parent, "CreateGameServerPage")
{
	m_pMapList = new ComboBox(this, "MapList", 12, false);
	m_pEnableBots = new CheckButton(this, "EnableBotsCheck", "");
	for (int i = 0; i < 4; ++i)
		m_pSkill[i] = new RadioButton(this, kSkillFields[i], "");
	m_pIdentity = new CCreateGameSettingsList(this, "IdentitySettings", csretro::ScrGroup::Identity);

	LoadControlSettings("resource/CreateGameServerPage.res");

	m_pBotHint = dynamic_cast<Label *>(FindChildByName("BotNavHint", true));

	LoadMapList();
}

CCreateGameServerPage::~CCreateGameServerPage() = default;

void CCreateGameServerPage::LoadMapList()
{
	m_maps.clear();
	if (m_pMapList)
		m_pMapList->DeleteAllItems();
	if (!::g_pFullFileSystem)
	{
		Menu_Con("CSRETRO_CREATE_MAPS 0 (kein Filesystem)");
		return;
	}

	FileFindHandle_t handle = 0;
	for (const char *file = ::g_pFullFileSystem->FindFirst("maps/*.bsp", &handle, "GAME");
		file; file = ::g_pFullFileSystem->FindNext(handle))
	{
		std::string name = file;
		const size_t dot = name.rfind('.');
		if (dot == std::string::npos)
			continue;
		name.erase(dot);
		if (name.empty())
			continue;

		MapEntry entry;
		entry.name = name;
		// Bots ohne Nav-Mesh spawnen zwar, laufen aber nicht — das muss die UI zeigen.
		const std::string nav = "maps/" + name + ".nav";
		entry.hasNav = ::g_pFullFileSystem->FileExists(nav.c_str());
		m_maps.push_back(entry);
	}
	::g_pFullFileSystem->FindClose(handle);

	std::sort(m_maps.begin(), m_maps.end(), [](const MapEntry &a, const MapEntry &b) {
		return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
	});
	m_maps.erase(std::unique(m_maps.begin(), m_maps.end(),
			      [](const MapEntry &a, const MapEntry &b) {
				      return strcasecmp(a.name.c_str(), b.name.c_str()) == 0;
			      }),
		m_maps.end());

	for (const MapEntry &entry : m_maps)
	{
		KeyValues *data = new KeyValues("data", "mapname", entry.name.c_str());
		m_pMapList->AddItem(entry.name.c_str(), data);
		data->deleteThis();
	}

	int navCount = 0;
	for (const MapEntry &entry : m_maps)
		navCount += entry.hasNav ? 1 : 0;
	Menu_Con("CSRETRO_CREATE_MAPS %d nav=%d", static_cast<int>(m_maps.size()), navCount);
}

const CCreateGameServerPage::MapEntry *CCreateGameServerPage::SelectedEntry() const
{
	if (!m_pMapList)
		return nullptr;
	const int item = m_pMapList->GetActiveItem();
	if (item < 0 || item >= static_cast<int>(m_maps.size()))
		return nullptr;
	return &m_maps[static_cast<size_t>(item)];
}

void CCreateGameServerPage::UpdateBotControls()
{
	const MapEntry *entry = SelectedEntry();
	const bool navOk = entry && entry->hasNav;
	const bool botsOn = navOk && m_pEnableBots && m_pEnableBots->IsSelected();

	if (m_pEnableBots)
	{
		// Ohne Nav-Mesh wäre „Bots aktivieren“ eine Option ohne Wirkung.
		m_pEnableBots->SetEnabled(navOk);
		// SilentSetSelected, weil SetSelected das CheckButtonChecked-Signal auch ohne
		// Zustandswechsel postet — der Handler landet sonst endlos wieder hier.
		if (!navOk && m_pEnableBots->IsSelected())
			m_pEnableBots->SilentSetSelected(false);
	}
	if (m_pBotHint)
		m_pBotHint->SetVisible(entry && !navOk);

	SetControlEnabled("BotQuotaCombo", botsOn);
	SetControlEnabled("BotLabel2", botsOn);
	SetControlEnabled("BotDifficultyLabel", botsOn);
	for (int i = 0; i < 4; ++i)
	{
		if (m_pSkill[i])
			m_pSkill[i]->SetEnabled(botsOn);
	}
}

void CCreateGameServerPage::OnResetData()
{
	const ServerProfile &p = gProfile;

	if (m_pMapList)
	{
		int index = -1;
		for (size_t i = 0; i < m_maps.size(); ++i)
		{
			if (!strcasecmp(m_maps[i].name.c_str(), p.map.c_str()))
			{
				index = static_cast<int>(i);
				break;
			}
		}
		m_pMapList->ActivateItem(index >= 0 ? index : 0);
	}

	char buf[8];
	snprintf(buf, sizeof(buf), "%d", p.bot_quota);
	SetControlString("BotQuotaCombo", buf);

	if (m_pIdentity)
		m_pIdentity->OnResetData();

	if (m_pEnableBots)
		m_pEnableBots->SilentSetSelected(p.bot_quota > 0);
	const int skill = ClampInt(p.bot_difficulty, 0, 3);
	for (int i = 0; i < 4; ++i)
	{
		if (m_pSkill[i])
			m_pSkill[i]->SetSelected(i == skill);
	}

	UpdateBotControls();
}

void CCreateGameServerPage::OnApplyChanges()
{
	ServerProfile &p = gProfile;

	// Identity zuerst: Bot-Quota hängt an maxplayers.
	if (m_pIdentity)
		m_pIdentity->OnApplyChanges();

	if (const MapEntry *entry = SelectedEntry())
		p.map = entry->name;

	char buf[128];
	const bool botsOn = m_pEnableBots && m_pEnableBots->IsSelected() && m_pEnableBots->IsEnabled();
	if (botsOn)
	{
		GetControlString("BotQuotaCombo", buf, sizeof(buf), "0");
		p.bot_quota = ClampInt(atoi(buf), 0, std::max(0, p.maxplayers - 1));
	}
	else
	{
		p.bot_quota = 0;
	}

	for (int i = 0; i < 4; ++i)
	{
		if (m_pSkill[i] && m_pSkill[i]->IsSelected())
			p.bot_difficulty = i;
	}

	Menu_Con("CSRETRO_CREATE_APPLY map=%s bots=%d diff=%d host=\"%s\" slots=%d",
		p.map.c_str(), p.bot_quota, p.bot_difficulty, p.hostname.c_str(), p.maxplayers);
}

void CCreateGameServerPage::OnTextChanged(Panel *panel)
{
	if (panel == m_pMapList)
		UpdateBotControls();
}

void CCreateGameServerPage::OnCheckButtonChecked()
{
	EnsureBotQuota(m_pEnableBots && m_pEnableBots->IsSelected());
	UpdateBotControls();
}

// Bots einschalten und dann 0 Bots bekommen wäre eine Option ohne Wirkung.
void CCreateGameServerPage::EnsureBotQuota(bool botsOn)
{
	if (!botsOn)
		return;

	char buf[8];
	GetControlString("BotQuotaCombo", buf, sizeof(buf), "0");
	if (atoi(buf) > 0)
		return;

	const int slots = ClampInt(gProfile.maxplayers, 2, 32);
	snprintf(buf, sizeof(buf), "%d", ClampInt(slots / 2, 1, slots - 1));
	SetControlString("BotQuotaCombo", buf);
}

bool CCreateGameServerPage::Gate_SelectMap(const char *name)
{
	if (!name || !m_pMapList)
		return false;
	for (size_t i = 0; i < m_maps.size(); ++i)
	{
		if (strcasecmp(m_maps[i].name.c_str(), name))
			continue;
		m_pMapList->ActivateItem(static_cast<int>(i));
		UpdateBotControls();
		return true;
	}
	return false;
}

const char *CCreateGameServerPage::Gate_SelectedMap() const
{
	const MapEntry *entry = SelectedEntry();
	return entry ? entry->name.c_str() : "";
}

bool CCreateGameServerPage::Gate_BotsAvailable() const
{
	return m_pEnableBots && m_pEnableBots->IsEnabled();
}

void CCreateGameServerPage::Gate_SetBotsEnabled(bool enabled)
{
	if (m_pEnableBots)
		m_pEnableBots->SilentSetSelected(enabled);
	EnsureBotQuota(enabled);
	UpdateBotControls();
}

int CCreateGameServerPage::Gate_IdentityCount() const
{
	return m_pIdentity ? m_pIdentity->Gate_RowCount() : 0;
}

bool CCreateGameServerPage::Gate_SetValue(const char *cvar, const char *value)
{
	return m_pIdentity && m_pIdentity->Gate_SetValue(cvar, value);
}

bool CCreateGameServerPage::Gate_AuditLabels()
{
	return m_pIdentity && m_pIdentity->Gate_AuditLabels();
}
