#include "OptionsSubKeyboard.h"

#include "OptionsClassicMetrics.h"
#include "Controls/MenuEngine.h"
#include "Controls/VControlsListPanel.h"

#include "../../vgui/xash_key_contract.h"
#include "../../vgui/menu_runtime_info.h"
#include "vgui_key_translation.h"
#include "../../src/menu_priv.h"

#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"
#include "vgui/ILocalize.h"
#include "vgui/IInputInternal.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_controls/SectionedListPanel.h"
#include "FileSystem.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

using namespace vgui2;

extern IFileSystem *g_pFullFileSystem;
extern vgui2::ILocalize *g_pVGuiLocalize;

namespace
{
bool BindingEquals(const char *a, const char *b)
{
	if (!a)
		a = "";
	if (!b)
		b = "";
	return !strcasecmp(a, b);
}

// Engine may store "cmd; wait" — match catalog command as first token.
// Also accept a few known catalog↔engine aliases (not destructive to custom binds).
bool BindingCommandAliases(const char *a, const char *b)
{
	if (!a || !b)
		return false;
	if (!strcasecmp(a, "centerview") && !strcasecmp(b, "force_centerview"))
		return true;
	if (!strcasecmp(a, "force_centerview") && !strcasecmp(b, "centerview"))
		return true;
	return false;
}

bool BindingMatchesCatalog(const char *engineBind, const char *catalog)
{
	if (BindingEquals(engineBind, catalog))
		return true;
	if (!engineBind || !catalog || !*catalog)
		return false;
	if (BindingCommandAliases(engineBind, catalog))
		return true;

	// Trim + optional surrounding quotes (matching only; custom binds stay intact in stage).
	char tmp[256];
	Q_strncpy(tmp, engineBind, sizeof(tmp));
	char *p = tmp;
	while (*p && isspace(static_cast<unsigned char>(*p)))
		++p;
	size_t len = std::strlen(p);
	while (len > 0 && isspace(static_cast<unsigned char>(p[len - 1])))
		p[--len] = '\0';
	if (len >= 2 && ((p[0] == '"' && p[len - 1] == '"') || (p[0] == '\'' && p[len - 1] == '\'')))
	{
		p[len - 1] = '\0';
		++p;
		len = std::strlen(p);
	}
	if (BindingEquals(p, catalog) || BindingCommandAliases(p, catalog))
		return true;

	const size_t n = std::strlen(catalog);
	if (strncasecmp(p, catalog, n) != 0)
		return false;
	const char c = p[n];
	return c == '\0' || c == ';' || isspace(static_cast<unsigned char>(c));
}

void LocalizeTokenToAnsi(const char *tokenOrText, char *out, int outSize)
{
	out[0] = '\0';
	if (!tokenOrText || !*tokenOrText || outSize <= 0)
		return;
	const char *tok = tokenOrText;
	if (tok[0] == '#')
		++tok;
	if (g_pVGuiLocalize && tokenOrText[0] == '#')
	{
		wchar_t *w = g_pVGuiLocalize->Find(tokenOrText);
		if (w && w[0])
		{
			g_pVGuiLocalize->ConvertUnicodeToANSI(w, out, outSize);
			if (out[0])
				return;
		}
	}
	Q_strncpy(out, tok, outSize);
}

const char *SkipWs(const char *p)
{
	while (p && *p && isspace(static_cast<unsigned char>(*p)))
		++p;
	return p;
}

// Minimal quoted-token parser for kb_*.lst (COM_Parse-compatible enough).
const char *ParseToken(const char *data, char *out, int outSize)
{
	out[0] = '\0';
	data = SkipWs(data);
	if (!data || !*data)
		return data;
	if (*data == '\"')
	{
		++data;
		int n = 0;
		while (*data && *data != '\"' && n < outSize - 1)
			out[n++] = *data++;
		out[n] = '\0';
		if (*data == '\"')
			++data;
		return data;
	}
	int n = 0;
	while (*data && !isspace(static_cast<unsigned char>(*data)) && n < outSize - 1)
		out[n++] = *data++;
	out[n] = '\0';
	return data;
}
} // namespace

COptionsSubKeyboard::COptionsSubKeyboard(Panel *parent) : PropertyPage(parent, "OptionsSubKeyboard")
{
	CreateKeyBindingList();
	m_pSetBindingButton = new Button(this, "ChangeKeyButton", "");
	m_pClearBindingButton = new Button(this, "ClearKeyButton", "");
	LoadControlSettings("resource/OptionsSubKeyboard.res");
	CaptureDesignRects();
	OwnClassicControlPins();
	ParseActionDescriptions();
	m_pSetBindingButton->SetEnabled(false);
	m_pClearBindingButton->SetEnabled(false);
	SnapshotEngine();
	FillInCurrentBindings();
}

COptionsSubKeyboard::~COptionsSubKeyboard() = default;

void COptionsSubKeyboard::CreateKeyBindingList()
{
	m_pKeyBindList = new VControlsListPanel(this, "listpanel_keybindlist");
	m_pKeyBindList->AddActionSignalTarget(this);
}

void COptionsSubKeyboard::OnPageShow()
{
	// Switching tabs is navigation, not Cancel. Keep staged edits until the
	// dialog explicitly applies or closes; otherwise Keyboard -> Mouse ->
	// Keyboard silently reverted the user's new bindings.
	if (!HasPendingChanges())
		SnapshotEngine();
	else
		CsretroMenu_CaptureLog("OnPageShow preserve staged bindings");
	FillInCurrentBindings();
	if (CsretroMenu_CaptureDebugEnabled())
	{
		int boundKeys = 0;
		int mappedActions = 0;
		for (int i = 0; i < MenuEngine::KeyCount(); ++i)
		{
			if (!m_snapshot[static_cast<size_t>(i)].empty())
				++boundKeys;
		}
		for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
		{
			KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
			if (!item || !item->GetString("Binding", "")[0])
				continue;
			if (item->GetInt("PrimaryKeynum", -1) >= 0 || item->GetInt("AltKeynum", -1) >= 0)
				++mappedActions;
			CsretroMenu_CaptureLog("UI_ROW binding=%s primary=%s alt=%s",
				item->GetString("Binding", ""),
				item->GetString("Key", ""),
				item->GetString("AltKey", ""));
		}
		CsretroMenu_CaptureLog("BIND_SNAPSHOT A_keys_with_bind=%d B_actions_with_ui_slot=%d C_list_items=%d keycount=%d",
			boundKeys, mappedActions, m_pKeyBindList->GetItemCount(), MenuEngine::KeyCount());
		RunBindingAudit();
	}
}

void COptionsSubKeyboard::OnResetData()
{
	SnapshotEngine();
	FillInCurrentBindings();
	if (m_pKeyBindList->GetItemCount() > 0)
		m_pKeyBindList->SetSelectedItem(0);
}

void COptionsSubKeyboard::OnApplyChanges()
{
	ApplyDiffBindings();
	SnapshotEngine();
	FillInCurrentBindings();
	MenuEngine::ClientCmd("host_writeconfig\n");
}

void COptionsSubKeyboard::SnapshotEngine()
{
	const int n = MenuEngine::KeyCount();
	m_snapshot.assign(static_cast<size_t>(n), std::string());
	m_stage.assign(static_cast<size_t>(n), StagedKey{});
	for (int i = 0; i < n; ++i)
	{
		const char *b = MenuEngine::GetBinding(i);
		m_snapshot[static_cast<size_t>(i)] = b ? b : "";
		m_stage[static_cast<size_t>(i)].binding = m_snapshot[static_cast<size_t>(i)];
		m_stage[static_cast<size_t>(i)].dirty = false;
		m_stage[static_cast<size_t>(i)].slotHint = 0;
	}
}

bool COptionsSubKeyboard::HasPendingChanges() const
{
	if (m_stage.size() != m_snapshot.size())
		return false;
	for (size_t i = 0; i < m_stage.size(); ++i)
	{
		if (m_stage[i].dirty || m_stage[i].binding != m_snapshot[i])
			return true;
	}
	return false;
}

void COptionsSubKeyboard::MarkDirty()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

COptionsSubKeyboard::ActionAvail COptionsSubKeyboard::ClassifyAction(const char *binding) const
{
	if (!binding || !*binding)
		return ActionAvail::Unavailable;

	// Proven unavailable / no client command in CS Retro product path.
	if (!strcasecmp(binding, "lookat") || !strcasecmp(binding, "nightvision"))
		return ActionAvail::Unavailable;

	// Compatibility: present in classic catalog, engine may no-op but still bindable.
	if (!strcasecmp(binding, "+commandmenu"))
		return ActionAvail::Compatibility;

	return ActionAvail::Implemented;
}

void COptionsSubKeyboard::ParseActionDescriptions()
{
	m_sectionIndex = 0;
	// Clear previous sections/items if re-parsed
	if (m_pKeyBindList)
	{
		m_pKeyBindList->DeleteAllItems();
		m_pKeyBindList->RemoveAllSections();
	}
	AppendActionsFromFile("gfx/shell/kb_act.lst", false);
	AppendActionsFromFile("gfx/shell/kb_act_overlay.lst", true);
}

void COptionsSubKeyboard::AppendActionsFromFile(const char *path, bool isOverlay)
{
	if (!g_pFullFileSystem || !path)
		return;
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb", "GAME");
	if (fh == FILESYSTEM_INVALID_HANDLE)
		fh = g_pFullFileSystem->Open(path, "rb");
	if (fh == FILESYSTEM_INVALID_HANDLE)
	{
		if (!isOverlay)
			Menu_Con("CSRETRO_KEYBOARD missing %s", path);
		return;
	}
	const int size = g_pFullFileSystem->Size(fh);
	CUtlBuffer buf(0, size + 1, CUtlBuffer::TEXT_BUFFER);
	g_pFullFileSystem->Read(buf.Base(), size, fh);
	g_pFullFileSystem->Close(fh);
	char *base = static_cast<char *>(buf.Base());
	base[size] = '\0';

	const char *data = base;
	char binding[256];
	char description[256];
	while (data && *data)
	{
		data = ParseToken(data, binding, sizeof(binding));
		if (!binding[0])
			break;
		data = ParseToken(data, description, sizeof(description));
		if (!description[0])
			break;

		if (!strcasecmp(binding, "blank") && description[0] != '=')
		{
			++m_sectionIndex;
			constexpr int kScroll = 20;
			constexpr int kListRef = 480;
			const int listW = m_pKeyBindList->GetWide() > 0 ? m_pKeyBindList->GetWide() : kListRef;
			int usable = listW - kScroll - 4;
			if (usable < 180)
				usable = 180;
			const int actionW = usable * 45 / 100;
			const int keyW = (usable - actionW) / 2;
			const int altW = usable - actionW - keyW;
			m_pKeyBindList->AddSection(m_sectionIndex, description);
			m_pKeyBindList->AddColumnToSection(m_sectionIndex, "Action", description,
				SectionedListPanel::COLUMN_BRIGHT, actionW);
			m_pKeyBindList->AddColumnToSection(m_sectionIndex, "Key", "#GameUI_KeyButton",
				SectionedListPanel::COLUMN_BRIGHT, keyW);
			m_pKeyBindList->AddColumnToSection(m_sectionIndex, "AltKey", "#GameUI_Alternate",
				SectionedListPanel::COLUMN_BRIGHT, altW);
			continue;
		}
		if (!strcasecmp(binding, "blank"))
			continue;

		if (ClassifyAction(binding) == ActionAvail::Unavailable)
		{
			Menu_Con("CSRETRO_KEYBOARD skip_unavailable %s", binding);
			continue;
		}

		KeyValues *item = new KeyValues("Item");
		// #Valve_* Tokens bleiben; TextImage/SectionedListPanel lokalisiert zur Laufzeit.
		item->SetString("Action", description);
		item->SetString("Binding", binding);
		item->SetString("Key", "");
		item->SetString("AltKey", "");
		item->SetInt("PrimaryKeynum", -1);
		item->SetInt("AltKeynum", -1);
		m_pKeyBindList->AddItem(m_sectionIndex, item);
		item->deleteThis();
	}
}

void COptionsSubKeyboard::ClearBindItems()
{
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
	{
		const int id = m_pKeyBindList->GetItemIDFromRow(i);
		KeyValues *item = m_pKeyBindList->GetItemData(id);
		if (!item || !item->GetString("Binding", "")[0])
			continue;
		item->SetString("Key", "");
		item->SetString("AltKey", "");
		item->SetInt("PrimaryKeynum", -1);
		item->SetInt("AltKeynum", -1);
		m_pKeyBindList->InvalidateItem(id);
	}
}

KeyValues *COptionsSubKeyboard::GetItemForBinding(const char *binding)
{
	if (!binding || !*binding)
		return nullptr;
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if (!item)
			continue;
		if (BindingMatchesCatalog(binding, item->GetString("Binding", "")))
			return item;
	}
	return nullptr;
}

void COptionsSubKeyboard::AddBindingToItem(KeyValues *item, int keynum, int preferredColumn)
{
	if (!item || !XashKey::IsValidKeynum(keynum))
		return;
	if (!XashKey::IsUiCaptureable(keynum))
		return; // hidden extra — not shown in Primary/Alt

	const char *keyName = MenuEngine::KeynumToString(keynum);
	if (!keyName || !*keyName)
		return;

	const int primary = item->GetInt("PrimaryKeynum", -1);
	const int alt = item->GetInt("AltKeynum", -1);
	if (primary == keynum || alt == keynum)
		return;

	RemoveKeyFromItems(keynum);

	if (preferredColumn == 2)
	{
		item->SetInt("AltKeynum", keynum);
		item->SetString("AltKey", keyName);
		return;
	}
	if (preferredColumn == 1)
	{
		item->SetInt("PrimaryKeynum", keynum);
		item->SetString("Key", keyName);
		return;
	}

	if (primary < 0)
	{
		item->SetInt("PrimaryKeynum", keynum);
		item->SetString("Key", keyName);
	}
	else if (alt < 0)
	{
		item->SetInt("AltKeynum", keynum);
		item->SetString("AltKey", keyName);
	}
	else
	{
		// Shift: new key becomes primary; old primary → alt; old alt becomes hidden (stage keeps it until cleared by defaults/rebind)
		item->SetInt("AltKeynum", primary);
		item->SetString("AltKey", item->GetString("Key", ""));
		item->SetInt("PrimaryKeynum", keynum);
		item->SetString("Key", keyName);
	}
}

void COptionsSubKeyboard::RemoveKeyFromItems(int keynum)
{
	if (!XashKey::IsValidKeynum(keynum))
		return;
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
	{
		const int id = m_pKeyBindList->GetItemIDFromRow(i);
		KeyValues *item = m_pKeyBindList->GetItemData(id);
		if (!item)
			continue;
		if (item->GetInt("AltKeynum", -1) == keynum)
		{
			item->SetInt("AltKeynum", -1);
			item->SetString("AltKey", "");
			m_pKeyBindList->InvalidateItem(id);
		}
		if (item->GetInt("PrimaryKeynum", -1) == keynum)
		{
			item->SetInt("PrimaryKeynum", -1);
			item->SetString("Key", "");
			const int alt = item->GetInt("AltKeynum", -1);
			if (alt >= 0)
			{
				item->SetInt("PrimaryKeynum", alt);
				item->SetString("Key", item->GetString("AltKey", ""));
				item->SetInt("AltKeynum", -1);
				item->SetString("AltKey", "");
			}
			m_pKeyBindList->InvalidateItem(id);
		}
	}
}

void COptionsSubKeyboard::FillInCurrentBindings()
{
	ClearBindItems();
	const int n = MenuEngine::KeyCount();
	// Pass 1: explicit Primary/Alternate slot hints (cell-aware capture).
	for (int i = 0; i < n; ++i)
	{
		const StagedKey &sk = m_stage[static_cast<size_t>(i)];
		if (sk.binding.empty() || (sk.slotHint != 1 && sk.slotHint != 2))
			continue;
		KeyValues *item = GetItemForBinding(sk.binding.c_str());
		if (!item)
			continue;
		AddBindingToItem(item, i, sk.slotHint);
	}
	// Pass 2: classic auto-fill for remaining keys.
	for (int i = 0; i < n; ++i)
	{
		const StagedKey &sk = m_stage[static_cast<size_t>(i)];
		if (sk.binding.empty() || sk.slotHint == 1 || sk.slotHint == 2)
			continue;
		KeyValues *item = GetItemForBinding(sk.binding.c_str());
		if (!item)
			continue;
		AddBindingToItem(item, i, 0);
	}
	m_pKeyBindList->InvalidateLayout();
}

void COptionsSubKeyboard::FillInDefaultBindings()
{
	if (!g_pFullFileSystem)
		return;

	auto applyFile = [this](const char *path) {
		FileHandle_t fh = g_pFullFileSystem->Open(path, "rb", "GAME");
		if (fh == FILESYSTEM_INVALID_HANDLE)
			fh = g_pFullFileSystem->Open(path, "rb");
		if (fh == FILESYSTEM_INVALID_HANDLE)
			return;
		const int size = g_pFullFileSystem->Size(fh);
		CUtlBuffer buf(0, size + 1, CUtlBuffer::TEXT_BUFFER);
		g_pFullFileSystem->Read(buf.Base(), size, fh);
		g_pFullFileSystem->Close(fh);
		char *base = static_cast<char *>(buf.Base());
		base[size] = '\0';
		const char *data = base;
		char keyName[256];
		char binding[256];
		while (data && *data)
		{
			data = ParseToken(data, keyName, sizeof(keyName));
			if (!keyName[0])
				break;
			data = ParseToken(data, binding, sizeof(binding));
			if (!binding[0])
				break;
			const int keynum = MenuEngine::KeyNameToKeynum(keyName);
			if (!XashKey::IsValidKeynum(keynum))
				continue;
			if (XashKey::IsReserved(keynum))
				continue;
			if (ClassifyAction(binding) == ActionAvail::Unavailable)
				continue;
			m_stage[static_cast<size_t>(keynum)].binding = binding;
			m_stage[static_cast<size_t>(keynum)].dirty = true;
			m_stage[static_cast<size_t>(keynum)].slotHint = 0;
		}
	};

	// Reset staged bindings that belong to catalog actions (captureable + hidden extras).
	for (int i = 0; i < MenuEngine::KeyCount(); ++i)
	{
		const std::string &b = m_stage[static_cast<size_t>(i)].binding;
		if (b.empty())
			continue;
		if (GetItemForBinding(b.c_str()))
		{
			if (!XashKey::IsReserved(i))
			{
				m_stage[static_cast<size_t>(i)].binding.clear();
				m_stage[static_cast<size_t>(i)].dirty = true;
				m_stage[static_cast<size_t>(i)].slotHint = 0;
			}
		}
	}

	applyFile("gfx/shell/kb_def.lst");
	applyFile("gfx/shell/kb_def_overlay.lst");

	// Reserved policy wins over historical defaults.
	for (int i = 0; i < MenuEngine::KeyCount(); ++i)
	{
		if (!XashKey::IsReserved(i))
			continue;
		const char *req = XashKey::ReservedBinding(i);
		m_stage[static_cast<size_t>(i)].binding = req ? req : "cancelselect";
		m_stage[static_cast<size_t>(i)].dirty = true;
		m_stage[static_cast<size_t>(i)].slotHint = 0;
	}

	FillInCurrentBindings();
	MarkDirty();
}

void COptionsSubKeyboard::ApplyDiffBindings()
{
	for (int i = 0; i < MenuEngine::KeyCount(); ++i)
	{
		StagedKey &st = m_stage[static_cast<size_t>(i)];
		const std::string &snap = m_snapshot[static_cast<size_t>(i)];
		if (!st.dirty && st.binding == snap)
			continue;
		MenuEngine::SetBinding(i, st.binding.c_str());
		st.dirty = false;
	}
}

void COptionsSubKeyboard::OnCommand(const char *command)
{
	if (!strcasecmp(command, "Defaults"))
	{
		QueryBox *box = new QueryBox("#GameUI_KeyboardSettings", "#GameUI_KeyboardSettingsText", this);
		box->AddActionSignalTarget(this);
		box->SetOKCommand(new KeyValues("DefaultsOK"));
		box->DoModal();
		return;
	}
	if (!strcasecmp(command, "ChangeKey") && m_pKeyBindList && !m_pKeyBindList->IsCapturing())
	{
		CsretroMenu_CaptureLog("OnCommand ChangeKey");
		BeginCapture();
		return;
	}
	if (!strcasecmp(command, "ClearKey") && m_pKeyBindList && !m_pKeyBindList->IsCapturing())
	{
		OnKeyCodePressed(KEY_DELETE);
		m_pKeyBindList->RequestFocus();
		return;
	}
	BaseClass::OnCommand(command);
}

void COptionsSubKeyboard::BeginCapture(int preferredColumn)
{
	if (!m_pKeyBindList)
	{
		CsretroMenu_CaptureLog("BeginCapture FAIL no_list");
		return;
	}
	if (m_pKeyBindList->IsCapturing())
	{
		CsretroMenu_CaptureLog("BeginCapture already capturing");
		return;
	}
	const int selected = m_pKeyBindList->GetSelectedItem();
	if (!m_pKeyBindList->IsItemIDValid(selected))
	{
		CsretroMenu_CaptureLog("BeginCapture FAIL invalid_selection id=%d", selected);
		return;
	}
	KeyValues *item = m_pKeyBindList->GetItemData(selected);
	if (!item || !item->GetString("Binding", "")[0])
	{
		CsretroMenu_CaptureLog("BeginCapture FAIL no_binding id=%d", selected);
		return;
	}

	int column = preferredColumn;
	if (column != 1 && column != 2)
		column = ResolveEditKeyColumn(item);

	m_pKeyBindList->SetItemOfInterest(selected);
	m_captureItemId = selected;
	Q_strncpy(m_captureSavedKey, item->GetString("Key", ""), sizeof(m_captureSavedKey));
	Q_strncpy(m_captureSavedAlt, item->GetString("AltKey", ""), sizeof(m_captureSavedAlt));

	m_pKeyBindList->StartCaptureMode(column, dc_blank);
	ShowCaptureWaitingState();
	// Shared contract: printables must arrive as Xash keynums on Key_Event, not only as Char.
	MenuEngine::EnableTextInput(false);
	ArmInitiatingMouseGate();
	CsretroMenu_CaptureLog("BeginCapture OK id=%d binding=%s col=%d capturing=%d initMouseMask=0x%x extApi=%d",
		selected, item->GetString("Binding", ""), column, IsCapturing() ? 1 : 0, m_initiatingMouseMask,
		MenuEngine::HasExtendedEngfuncs() ? 1 : 0);
}

int COptionsSubKeyboard::ResolveEditKeyColumn(KeyValues *item) const
{
	if (!item)
		return 1;
	if (item->GetInt("PrimaryKeynum", -1) < 0)
		return 1;
	if (item->GetInt("AltKeynum", -1) < 0)
		return 2;
	return 1; // both filled — classic: rebind Primary
}

void COptionsSubKeyboard::ShowCaptureWaitingState()
{
	if (!m_pKeyBindList || m_captureItemId < 0)
		return;
	KeyValues *item = m_pKeyBindList->GetItemData(m_captureItemId);
	if (!item)
		return;
	// Store #token — SectionedListPanel/TextImage and inline Label localize for display.
	const char *waitToken = "#GameUI_PressAKey";
	const int col = m_pKeyBindList->GetCaptureColumn();
	if (col == 2)
		item->SetString("AltKey", waitToken);
	else
		item->SetString("Key", waitToken);
	m_pKeyBindList->InvalidateItem(m_captureItemId);
	CsretroMenu_CaptureLog("WaitingState id=%d col=%d text=%s", m_captureItemId, col, waitToken);
}

void COptionsSubKeyboard::RestoreCaptureSlotText()
{
	if (!m_pKeyBindList || m_captureItemId < 0)
		return;
	KeyValues *item = m_pKeyBindList->GetItemData(m_captureItemId);
	if (!item)
		return;
	item->SetString("Key", m_captureSavedKey);
	item->SetString("AltKey", m_captureSavedAlt);
	m_pKeyBindList->InvalidateItem(m_captureItemId);
}

void COptionsSubKeyboard::ArmInitiatingMouseGate()
{
	// Only suppress buttons that are still physically down when capture starts.
	// Edit key / Command fire after MouseUp → mask stays 0 (no phantom LEFT).
	// DoubleClick / MouseDown while button held → that press/release cycle is suppressed.
	// Enter / keyboard start → no mouse gate.
	m_initiatingMouseMask = 0;
	vgui2::IInput *in = input();
	const vgui2::MouseCode codes[] = {
		MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_4, MOUSE_5};
	for (vgui2::MouseCode c : codes)
	{
		if (in && in->IsMouseDown(c))
			m_initiatingMouseMask |= (1u << MouseCodeBit(c));
	}
	CsretroMenu_CaptureLog("ArmInitiatingMouseGate mask=0x%x", m_initiatingMouseMask);
}

int COptionsSubKeyboard::MouseCodeBit(MouseCode code)
{
	return static_cast<int>(code);
}

bool COptionsSubKeyboard::ShouldSuppressCaptureMouse(MouseCode code) const
{
	if (!IsCapturing() || m_initiatingMouseMask == 0)
		return false;
	const unsigned bit = 1u << MouseCodeBit(code);
	return (m_initiatingMouseMask & bit) != 0;
}

void COptionsSubKeyboard::NoteCaptureMouseReleased(MouseCode code)
{
	if (m_initiatingMouseMask == 0)
		return;
	const unsigned bit = 1u << MouseCodeBit(code);
	if (m_initiatingMouseMask & bit)
	{
		m_initiatingMouseMask &= ~bit;
		CsretroMenu_CaptureLog("InitiatingMouse released code=%d mask=0x%x",
			static_cast<int>(code), m_initiatingMouseMask);
	}
}

void COptionsSubKeyboard::CancelCapture()
{
	if (!m_pKeyBindList || !m_pKeyBindList->IsCapturing())
		return;
	CsretroMenu_CaptureLog("CancelCapture");
	m_initiatingMouseMask = 0;
	RestoreCaptureSlotText();
	m_pKeyBindList->EndCaptureMode(dc_arrow);
	m_captureItemId = -1;
}

bool COptionsSubKeyboard::IsCapturing() const
{
	return m_pKeyBindList && m_pKeyBindList->IsCapturing();
}

bool COptionsSubKeyboard::OnRawXashKey(int keynum, bool down)
{
	CsretroMenu_CaptureLog("OnRawXashKey enter key=%d down=%d capturing=%d initMask=0x%x",
		keynum, down ? 1 : 0, IsCapturing() ? 1 : 0, m_initiatingMouseMask);
	if (!IsCapturing())
		return false;

	if (keynum == K_ESCAPE)
	{
		if (down)
			CancelCapture();
		return true;
	}

	const bool isMouse =
		keynum == K_MOUSE1 || keynum == K_MOUSE2 || keynum == K_MOUSE3 ||
		keynum == K_MOUSE4 || keynum == K_MOUSE5;
	const bool isWheel = keynum == K_MWHEELUP || keynum == K_MWHEELDOWN;

	if (isMouse)
	{
		MouseCode code = MOUSE_LEFT;
		if (keynum == K_MOUSE2)
			code = MOUSE_RIGHT;
		else if (keynum == K_MOUSE3)
			code = MOUSE_MIDDLE;
		else if (keynum == K_MOUSE4)
			code = MOUSE_4;
		else if (keynum == K_MOUSE5)
			code = MOUSE_5;

		if (!down)
		{
			NoteCaptureMouseReleased(code);
			return true; // consume — do not let VGUI also treat this as capture UI noise
		}
		if (ShouldSuppressCaptureMouse(code))
		{
			CsretroMenu_CaptureLog("OnRaw suppress initiating mouse key=%d", keynum);
			return true; // consume without FinishCapture
		}
		// Intentional mouse rebind after initiating gesture cleared.
		FinishCapture(keynum);
		return true;
	}

	if (isWheel)
	{
		CsretroMenu_CaptureLog("WHEEL_EVENT key=%d capturing=1 owner=bind", keynum);
		if (down)
			FinishCapture(keynum);
		return true;
	}

	// Keyboard / other captureable keys — Raw sink owns the event while capturing.
	if (!down)
		return true;
	FinishCapture(keynum);
	return true;
}

void COptionsSubKeyboard::CaptureDebug(const char *tag, int keynum, const char *extra) const
{
	const char *name = XashKey::IsValidKeynum(keynum) ? MenuEngine::KeynumToString(keynum) : "";
	const int interest = m_pKeyBindList ? m_pKeyBindList->GetItemOfInterest() : -1;
	KeyValues *item = (m_pKeyBindList && interest >= 0) ? m_pKeyBindList->GetItemData(interest) : nullptr;
	CsretroMenu_CaptureLog("%s keynum=%d name=%s capturing=%d interest=%d action=%s binding=%s extra=%s",
		tag ? tag : "?",
		keynum,
		name ? name : "",
		IsCapturing() ? 1 : 0,
		interest,
		item ? item->GetString("Action", "") : "",
		item ? item->GetString("Binding", "") : "",
		extra ? extra : "");
}

void COptionsSubKeyboard::OnDefaultsOK()
{
	FillInDefaultBindings();
	if (m_pKeyBindList)
		m_pKeyBindList->RequestFocus();
}

void COptionsSubKeyboard::ItemSelected(int itemID)
{
	m_pKeyBindList->SetItemOfInterest(itemID);
	const bool valid = m_pKeyBindList->IsItemIDValid(itemID);
	KeyValues *item = valid ? m_pKeyBindList->GetItemData(itemID) : nullptr;
	const bool hasAction = item && item->GetString("Binding", "")[0];
	m_pSetBindingButton->SetEnabled(hasAction);
	m_pClearBindingButton->SetEnabled(hasAction);
}

void COptionsSubKeyboard::ItemDoubleLeftClick(int itemID)
{
	CsretroMenu_CaptureLog("ItemDoubleLeftClick itemID=%d", itemID);
	if (m_pKeyBindList->IsItemIDValid(itemID))
	{
		m_pKeyBindList->SetSelectedItem(itemID);
		ItemSelected(itemID);
	}
	const int col = m_pKeyBindList->ResolveCaptureColumnAtCursor(itemID, 1);
	BeginCapture(col);
}

void COptionsSubKeyboard::OnKeyCodePressed(KeyCode code)
{
	if (code == KEY_ENTER && m_pKeyBindList && !m_pKeyBindList->IsCapturing())
	{
		BeginCapture();
		return;
	}
	if (code == KEY_DELETE && m_pKeyBindList && !m_pKeyBindList->IsCapturing())
	{
		const int id = m_pKeyBindList->GetSelectedItem();
		KeyValues *item = m_pKeyBindList->GetItemData(id);
		if (!item)
			return;
		const int primary = item->GetInt("PrimaryKeynum", -1);
		const int alt = item->GetInt("AltKeynum", -1);
		const char *binding = item->GetString("Binding", "");
		auto clearKey = [&](int keynum) {
			if (!XashKey::IsValidKeynum(keynum) || XashKey::IsReserved(keynum))
				return;
			if (BindingEquals(m_stage[static_cast<size_t>(keynum)].binding.c_str(), binding))
			{
				m_stage[static_cast<size_t>(keynum)].binding.clear();
				m_stage[static_cast<size_t>(keynum)].dirty = true;
				m_stage[static_cast<size_t>(keynum)].slotHint = 0;
			}
		};
		clearKey(primary);
		clearKey(alt);
		for (int i = 0; i < MenuEngine::KeyCount(); ++i)
		{
			if (i == primary || i == alt)
				continue;
			if (BindingEquals(m_stage[static_cast<size_t>(i)].binding.c_str(), binding) && !XashKey::IsReserved(i))
			{
				m_stage[static_cast<size_t>(i)].binding.clear();
				m_stage[static_cast<size_t>(i)].dirty = true;
				m_stage[static_cast<size_t>(i)].slotHint = 0;
			}
		}
		FillInCurrentBindings();
		MarkDirty();
		return;
	}
	BaseClass::OnKeyCodePressed(code);
}

void COptionsSubKeyboard::OnKeyCodeTyped(KeyCode code)
{
	// While capturing, keyboard keys are owned by OnRawXashKey — do not double-Finish via VGUI.
	if (m_pKeyBindList && m_pKeyBindList->IsCapturing())
	{
		if (code == KEY_ESCAPE)
		{
			CancelCapture();
			return;
		}
		CsretroMenu_CaptureLog("OnKeyCodeTyped ignored while capturing (raw sink owns keys) code=%d",
			static_cast<int>(code));
		return;
	}
	if (code == KEY_ENTER)
	{
		OnKeyCodePressed(KEY_ENTER);
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

int COptionsSubKeyboard::MouseCodeToKeynum(MouseCode code)
{
	switch (code)
	{
	case MOUSE_LEFT:
		return K_MOUSE1;
	case MOUSE_RIGHT:
		return K_MOUSE2;
	case MOUSE_MIDDLE:
		return K_MOUSE3;
	case MOUSE_4:
		return K_MOUSE4;
	case MOUSE_5:
		return K_MOUSE5;
	default:
		return -1;
	}
}

void COptionsSubKeyboard::OnMousePressed(MouseCode code)
{
	if (m_pKeyBindList && m_pKeyBindList->IsCapturing())
	{
		if (ShouldSuppressCaptureMouse(code))
		{
			CsretroMenu_CaptureLog("OnMousePressed suppress initiating code=%d", static_cast<int>(code));
			return;
		}
		FinishCapture(MouseCodeToKeynum(code));
		return;
	}
	BaseClass::OnMousePressed(code);
}

void COptionsSubKeyboard::OnMouseDoublePressed(MouseCode code)
{
	if (m_pKeyBindList && m_pKeyBindList->IsCapturing())
	{
		if (ShouldSuppressCaptureMouse(code))
		{
			CsretroMenu_CaptureLog("OnMouseDoublePressed suppress initiating code=%d", static_cast<int>(code));
			return;
		}
		FinishCapture(MouseCodeToKeynum(code));
		return;
	}
	BaseClass::OnMouseDoublePressed(code);
}

void COptionsSubKeyboard::OnMouseWheeled(int delta)
{
	if (m_pKeyBindList && m_pKeyBindList->IsCapturing())
	{
		FinishCapture(delta > 0 ? K_MWHEELUP : K_MWHEELDOWN);
		return;
	}
	BaseClass::OnMouseWheeled(delta);
}

void COptionsSubKeyboard::PromptReplace(int keynum, const char *existingBinding, const char *newActionLabel)
{
	m_pendingReplaceKeynum = keynum;
	m_pendingReplaceItem = m_pKeyBindList ? m_pKeyBindList->GetItemOfInterest() : -1;
	// Capture column already frozen by FinishCapture into m_pendingReplaceColumn when set by caller.

	const char *keyName = MenuEngine::KeynumToString(keynum);
	KeyValues *oldItem = GetItemForBinding(existingBinding);
	const char *oldToken = oldItem ? oldItem->GetString("Action", "") : nullptr;

	char oldAnsi[128];
	char newAnsi[128];
	LocalizeTokenToAnsi(oldToken && oldToken[0] ? oldToken : existingBinding, oldAnsi, sizeof(oldAnsi));
	LocalizeTokenToAnsi(newActionLabel, newAnsi, sizeof(newAnsi));
	if (!oldAnsi[0])
		Q_strncpy(oldAnsi, existingBinding && existingBinding[0] ? existingBinding : "?", sizeof(oldAnsi));
	if (!newAnsi[0])
		Q_strncpy(newAnsi, "?", sizeof(newAnsi));

	char templateAnsi[256];
	LocalizeTokenToAnsi("#GameUI_KeyReplacePrompt", templateAnsi, sizeof(templateAnsi));
	char msg[384];
	if (templateAnsi[0] && std::strstr(templateAnsi, "%s1"))
	{
		Q_strncpy(msg, templateAnsi, sizeof(msg));
		auto replaceOnce = [](char *buf, int bufSize, const char *token, const char *value) {
			char *at = std::strstr(buf, token);
			if (!at)
				return;
			char tmp[384];
			const int head = static_cast<int>(at - buf);
			std::snprintf(tmp, sizeof(tmp), "%.*s%s%s", head, buf, value, at + std::strlen(token));
			Q_strncpy(buf, tmp, bufSize);
		};
		replaceOnce(msg, sizeof(msg), "%s1", keyName && keyName[0] ? keyName : "?");
		replaceOnce(msg, sizeof(msg), "%s2", oldAnsi);
		replaceOnce(msg, sizeof(msg), "%s3", newAnsi);
	}
	else
	{
		std::snprintf(msg, sizeof(msg),
			"\"%s\" is currently bound to \"%s\". Replace it with \"%s\"?",
			keyName && keyName[0] ? keyName : "?",
			oldAnsi,
			newAnsi);
	}

	QueryBox *box = new QueryBox("#GameUI_KeyboardSettings", msg, this);
	box->AddActionSignalTarget(this);
	box->SetOKCommand(new KeyValues("ReplaceOK"));
	box->SetCancelCommand(new KeyValues("ReplaceCancel"));
	box->DoModal();
}

void COptionsSubKeyboard::OnReplaceCancel()
{
	CaptureDebug("replace_cancel", m_pendingReplaceKeynum, nullptr);
	m_pendingReplaceKeynum = -1;
	m_pendingReplaceItem = -1;
	m_pendingReplaceColumn = 1;
}

void COptionsSubKeyboard::OnReplaceOK()
{
	const int keynum = m_pendingReplaceKeynum;
	const int itemId = m_pendingReplaceItem;
	const int column = m_pendingReplaceColumn;
	m_pendingReplaceKeynum = -1;
	m_pendingReplaceItem = -1;
	m_pendingReplaceColumn = 1;
	if (!m_pKeyBindList || !XashKey::IsValidKeynum(keynum))
		return;
	if (itemId >= 0)
		m_pKeyBindList->SetItemOfInterest(itemId);
	KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemOfInterest());
	if (!item)
		return;
	const char *binding = item->GetString("Binding", "");
	if (!binding[0])
		return;
	CaptureDebug("replace_ok", keynum, binding);
	ApplyCapturedKey(keynum, item, binding, column);
}

void COptionsSubKeyboard::ApplyCapturedKey(int keynum, KeyValues *item, const char *binding, int column)
{
	if (!item || !binding || !*binding || !XashKey::IsValidKeynum(keynum))
		return;
	if (!XashKey::IsUiCaptureable(keynum) || XashKey::IsReserved(keynum))
		return;

	if (column != 1 && column != 2)
		column = 1;

	const int oldPrimary = item->GetInt("PrimaryKeynum", -1);
	const int oldAlt = item->GetInt("AltKeynum", -1);
	const int displaced = (column == 2) ? oldAlt : oldPrimary;

	if (XashKey::IsValidKeynum(displaced) && displaced != keynum &&
		BindingEquals(m_stage[static_cast<size_t>(displaced)].binding.c_str(), binding))
	{
		m_stage[static_cast<size_t>(displaced)].binding.clear();
		m_stage[static_cast<size_t>(displaced)].dirty = true;
		m_stage[static_cast<size_t>(displaced)].slotHint = 0;
	}

	m_stage[static_cast<size_t>(keynum)].binding = binding;
	m_stage[static_cast<size_t>(keynum)].dirty = true;
	m_stage[static_cast<size_t>(keynum)].slotHint = static_cast<char>(column);

	FillInCurrentBindings();
	MarkDirty();
	CaptureDebug("staged", keynum, column == 2 ? item->GetString("AltKey", "") : item->GetString("Key", ""));
}

void COptionsSubKeyboard::FinishCapture(int keynum)
{
	if (!m_pKeyBindList || !m_pKeyBindList->IsCapturing())
		return;

	const int r = m_pKeyBindList->GetItemOfInterest();
	KeyValues *item = m_pKeyBindList->GetItemData(r);
	const char *binding = item ? item->GetString("Binding", "") : "";
	const int column = m_pKeyBindList->GetCaptureColumn();

	CaptureDebug("finish", keynum, binding);

	if (!item || !binding[0])
	{
		RestoreCaptureSlotText();
		m_pKeyBindList->EndCaptureMode(dc_arrow);
		m_captureItemId = -1;
		return;
	}
	if (!XashKey::IsValidKeynum(keynum) || !XashKey::IsUiCaptureable(keynum))
	{
		CaptureDebug("reject_invalid", keynum, nullptr);
		return;
	}
	if (XashKey::IsReserved(keynum))
	{
		CaptureDebug("reject_reserved", keynum, nullptr);
		return;
	}

	RestoreCaptureSlotText();
	m_pKeyBindList->EndCaptureMode(dc_arrow);
	m_captureItemId = -1;

	const std::string &existing = m_stage[static_cast<size_t>(keynum)].binding;
	if (!existing.empty() && !BindingEquals(existing.c_str(), binding))
	{
		m_pKeyBindList->SetItemOfInterest(r);
		m_pendingReplaceColumn = column;
		PromptReplace(keynum, existing.c_str(), item->GetString("Action", ""));
		m_initiatingMouseMask = 0;
		return;
	}

	ApplyCapturedKey(keynum, item, binding, column);
	m_initiatingMouseMask = 0;
}

void COptionsSubKeyboard::PerformLayout()
{
	BaseClass::PerformLayout();
	CaptureDesignRects();
	ApplyAdaptiveListLayout();
	RefreshColumnWidths();
}

void COptionsSubKeyboard::OwnClassicControlPins()
{
	// .res pins are measured against the 64×24 PropertyPage default — discard them.
	auto pinFixed = [](Panel *p) {
		if (!p)
			return;
		int x = 0, y = 0;
		p->GetPos(x, y);
		p->SetPinCorner(Panel::PIN_TOPLEFT, x, y);
	};
	pinFixed(m_pKeyBindList);
	pinFixed(FindChildByName("Defaults"));
	pinFixed(m_pSetBindingButton);
	pinFixed(m_pClearBindingButton);
}

void COptionsSubKeyboard::CaptureDesignRects()
{
	auto takeOrClassic = [](Panel *p, CsretroOptionsLayout::Rect &out, int x, int y, int w, int h) {
		if (out.valid)
			return;
		CsretroOptionsLayout::CaptureRect(p, out);
		if (!out.valid || out.w < 8 || out.h < 8)
			out = {x, y, w, h, true};
	};
	takeOrClassic(m_pKeyBindList, m_designList,
		CsretroOptionsClassic::kKeyboardListX, CsretroOptionsClassic::kKeyboardListY,
		CsretroOptionsClassic::kKeyboardListW, CsretroOptionsClassic::kKeyboardListH);
	takeOrClassic(FindChildByName("Defaults"), m_designDefaults,
		CsretroOptionsClassic::kKeyboardDefaultsX, CsretroOptionsClassic::kKeyboardDefaultsY,
		CsretroOptionsClassic::kKeyboardDefaultsW, CsretroOptionsClassic::kKeyboardDefaultsH);
	takeOrClassic(m_pSetBindingButton, m_designChange,
		CsretroOptionsClassic::kKeyboardChangeX, CsretroOptionsClassic::kKeyboardChangeY,
		CsretroOptionsClassic::kKeyboardChangeW, CsretroOptionsClassic::kKeyboardChangeH);
	takeOrClassic(m_pClearBindingButton, m_designClear,
		CsretroOptionsClassic::kKeyboardClearX, CsretroOptionsClassic::kKeyboardClearY,
		CsretroOptionsClassic::kKeyboardClearW, CsretroOptionsClassic::kKeyboardClearH);
}

void COptionsSubKeyboard::ApplyAdaptiveListLayout()
{
	Panel *dlg = CsretroOptionsLayout::FindOptionsDialog(this);
	int dw = 0, dh = 0;
	if (dlg)
		dlg->GetSize(dw, dh);
	const int extraW = dw - CsretroOptionsClassic::kPreferredWide;
	const int extraH = dh - CsretroOptionsClassic::kPreferredTall;
	CsretroOptionsLayout::ApplyKeyboardGrow(this, m_pKeyBindList, FindChildByName("Defaults"),
		m_pSetBindingButton, m_pClearBindingButton, m_designList, m_designDefaults,
		m_designChange, m_designClear, extraW, extraH);
}

void COptionsSubKeyboard::RefreshColumnWidths()
{
	if (!m_pKeyBindList)
		return;
	const int listW = m_pKeyBindList->GetWide();
	if (listW == m_lastColumnWide && m_lastColumnWide > 0)
		return;
	m_lastColumnWide = listW;
	constexpr int kScroll = 20;
	int usable = listW - kScroll - 4;
	if (usable < 180)
		usable = 180;
	const int actionW = usable * 45 / 100;
	const int keyW = (usable - actionW) / 2;
	const int altW = usable - actionW - keyW;
	for (int s = 1; s <= m_sectionIndex; ++s)
	{
		m_pKeyBindList->SetColumnWidth(s, "Action", actionW);
		m_pKeyBindList->SetColumnWidth(s, "Key", keyW);
		m_pKeyBindList->SetColumnWidth(s, "AltKey", altW);
	}
}

void COptionsSubKeyboard::Gate_ListBounds(int &x, int &y, int &w, int &h) const
{
	x = y = w = h = 0;
	if (m_pKeyBindList)
		m_pKeyBindList->GetBounds(x, y, w, h);
}

bool COptionsSubKeyboard::Gate_ListVisibleChildrenContained() const
{
	return m_pKeyBindList && m_pKeyBindList->AreVisibleChildrenContained();
}

namespace
{
vgui2::Panel *FindQueryBoxRecursive(vgui2::Panel *p)
{
	if (!p)
		return nullptr;
	if (dynamic_cast<vgui2::QueryBox *>(p) && p->IsVisible())
		return p;
	for (int i = 0; i < p->GetChildCount(); ++i)
	{
		if (vgui2::Panel *hit = FindQueryBoxRecursive(p->GetChild(i)))
			return hit;
	}
	return nullptr;
}
} // namespace

vgui2::Panel *COptionsSubKeyboard::FindVisibleQueryBox() const
{
	if (vgui2::Panel *hit = FindQueryBoxRecursive(const_cast<COptionsSubKeyboard *>(this)))
		return hit;
	if (vgui2::Panel *dlg = CsretroOptionsLayout::FindOptionsDialog(const_cast<COptionsSubKeyboard *>(this)))
		return FindQueryBoxRecursive(dlg);
	return nullptr;
}

bool COptionsSubKeyboard::Gate_OpenDefaultsQuery()
{
	OnCommand("Defaults");
	return Gate_HasQueryBox();
}

bool COptionsSubKeyboard::Gate_HasQueryBox() const
{
	return FindVisibleQueryBox() != nullptr;
}

bool COptionsSubKeyboard::Gate_DismissQueryBox()
{
	vgui2::Panel *box = FindVisibleQueryBox();
	if (!box)
		return false;
	box->OnCommand("Cancel");
	return true;
}

int COptionsSubKeyboard::Gate_VisibleActionCount() const
{
	int n = 0;
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if (item && item->GetString("Binding", "")[0])
			++n;
	}
	return n;
}

bool COptionsSubKeyboard::Gate_SelectActionByBinding(const char *binding)
{
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
	{
		const int id = m_pKeyBindList->GetItemIDFromRow(i);
		KeyValues *item = m_pKeyBindList->GetItemData(id);
		if (item && BindingEquals(item->GetString("Binding", ""), binding))
		{
			m_pKeyBindList->SetSelectedItem(id);
			ItemSelected(id);
			return true;
		}
	}
	return false;
}

bool COptionsSubKeyboard::Gate_StartCapture()
{
	BeginCapture();
	return IsCapturing();
}

bool COptionsSubKeyboard::Gate_StartCaptureColumn(int column)
{
	BeginCapture(column);
	return IsCapturing();
}

bool COptionsSubKeyboard::Gate_ClearSelected()
{
	OnKeyCodePressed(KEY_DELETE);
	return true;
}

bool COptionsSubKeyboard::Gate_IsCapturing() const
{
	return IsCapturing();
}

unsigned COptionsSubKeyboard::Gate_InitiatingMouseMask() const
{
	return m_initiatingMouseMask;
}

bool COptionsSubKeyboard::Gate_FinishCaptureKeynum(int keynum)
{
	if (!IsCapturing())
		return false;
	FinishCapture(keynum);
	return !IsCapturing() || m_pendingReplaceKeynum >= 0;
}

const char *COptionsSubKeyboard::Gate_SelectedPrimaryKey() const
{
	const int id = m_pKeyBindList->GetSelectedItem();
	KeyValues *item = m_pKeyBindList->GetItemData(id);
	return item ? item->GetString("Key", "") : "";
}

const char *COptionsSubKeyboard::Gate_SelectedAltKey() const
{
	const int id = m_pKeyBindList->GetSelectedItem();
	KeyValues *item = m_pKeyBindList->GetItemData(id);
	return item ? item->GetString("AltKey", "") : "";
}

int COptionsSubKeyboard::Gate_ScrollValue() const
{
	if (!m_pKeyBindList || !m_pKeyBindList->GetScrollBar())
		return 0;
	return m_pKeyBindList->GetScrollBar()->GetValue();
}

int COptionsSubKeyboard::Gate_ScrollRange() const
{
	if (!m_pKeyBindList || !m_pKeyBindList->GetScrollBar())
		return 0;
	int mn = 0, mx = 0;
	m_pKeyBindList->GetScrollBar()->GetRange(mn, mx);
	const int win = m_pKeyBindList->GetScrollBar()->GetRangeWindow();
	const int span = mx - mn;
	if (span <= win)
		return 0;
	return span - win;
}

bool COptionsSubKeyboard::Gate_ListScreenCenter(int &x, int &y) const
{
	if (!m_pKeyBindList)
		return false;
	int w = 0, h = 0;
	m_pKeyBindList->GetSize(w, h);
	if (w <= 0 || h <= 0)
		return false;
	x = w / 2;
	y = (h > 40) ? 28 : h / 2;
	m_pKeyBindList->LocalToScreen(x, y);
	return true;
}

void COptionsSubKeyboard::RunBindingAudit() const
{
	static const char *kKeys[] = {
		"W", "A", "S", "D", "SPACE", "CTRL", "SHIFT", "E", "R",
		"MOUSE1", "MOUSE2", "MWHEELUP", "MWHEELDOWN", "`", "~",
	};

	// CONFIG = $XASH3D_BASEDIR/cstrike/config.cfg — same file Host_WriteConfig writes.
	// Do not search GAME/RODIR (that is not the engine user archive).
	char configAbs[1024];
	char userAbs[1024];
	char rcAbs[1024];
	char configBuf[65536];
	char userBuf[65536];
	int configLen = 0;
	int userLen = 0;
	const char *basedir = getenv("XASH3D_BASEDIR");
	if (!basedir || !*basedir)
		basedir = getenv("CSRETRO_RUN_DIR");
	const char *rodir = getenv("XASH3D_RODIR");
	configAbs[0] = userAbs[0] = rcAbs[0] = '\0';
	configBuf[0] = userBuf[0] = '\0';
	if (basedir && *basedir)
	{
		snprintf(configAbs, sizeof(configAbs), "%s/cstrike/config.cfg", basedir);
		snprintf(userAbs, sizeof(userAbs), "%s/cstrike/userconfig.cfg", basedir);
		snprintf(rcAbs, sizeof(rcAbs), "%s/cstrike/cstrike.rc", basedir);
		FILE *cf = fopen(configAbs, "rb");
		if (cf)
		{
			configLen = static_cast<int>(fread(configBuf, 1, sizeof(configBuf) - 1, cf));
			configBuf[configLen] = '\0';
			fclose(cf);
		}
		FILE *uf = fopen(userAbs, "rb");
		if (uf)
		{
			userLen = static_cast<int>(fread(userBuf, 1, sizeof(userBuf) - 1, uf));
			userBuf[userLen] = '\0';
			fclose(uf);
		}
	}

	// Exact key token (same as Host_WriteConfig / Key_KeynumToString). Last match wins.
	auto findBindInBuf = [](const char *buf, int len, const char *keyName, char *out, int outSize) {
		out[0] = '\0';
		if (!len || !buf || !keyName || !*keyName)
			return;
		const char *p = buf;
		while (p && *p)
		{
			while (*p && isspace(static_cast<unsigned char>(*p)))
				++p;
			if (!*p)
				break;
			if (strncasecmp(p, "bind", 4) != 0 || !isspace(static_cast<unsigned char>(p[4])))
			{
				while (*p && *p != '\n')
					++p;
				continue;
			}
			p = SkipWs(p + 4);
			char kn[64];
			char bn[256];
			p = ParseToken(p, kn, sizeof(kn));
			p = ParseToken(p, bn, sizeof(bn));
			if (kn[0] && !strcmp(kn, keyName))
				Q_strncpy(out, bn, outSize);
			while (*p && *p != '\n')
				++p;
		}
	};

	CsretroMenu_CaptureLog("BIND_AUDIT begin");
	CsretroMenu_CaptureLog("BIND_AUDIT context basedir=%s rodir=%s game=cstrike",
		basedir && *basedir ? basedir : "-",
		rodir && *rodir ? rodir : "-");
	CsretroMenu_CaptureLog("BIND_AUDIT config_path=%s readable=%d",
		configAbs[0] ? configAbs : "-", configLen > 0 ? 1 : 0);
	CsretroMenu_CaptureLog("BIND_AUDIT userconfig_path=%s readable=%d",
		userAbs[0] ? userAbs : "-", userLen > 0 ? 1 : 0);
	CsretroMenu_CaptureLog("BIND_AUDIT engine_exec rc=%s → config.cfg → userconfig.cfg → userconfigd",
		rcAbs[0] ? rcAbs : "-");
	for (const char *keyName : kKeys)
	{
		const int keynum = MenuEngine::KeyNameToKeynum(keyName);
		const char *canon = XashKey::IsValidKeynum(keynum) ? MenuEngine::KeynumToString(keynum) : "";
		if (!canon || !canon[0])
			canon = keyName;
		char cfg[256];
		char ucfg[256];
		findBindInBuf(configBuf, configLen, canon, cfg, sizeof(cfg));
		findBindInBuf(userBuf, userLen, canon, ucfg, sizeof(ucfg));
		const char *engine = "";
		if (XashKey::IsValidKeynum(keynum))
		{
			const char *b = MenuEngine::GetBinding(keynum);
			engine = b ? b : "";
		}
		const char *uiSlot = "unmatched";
		const char *uiAction = "";
		if (XashKey::IsValidKeynum(keynum) && m_pKeyBindList)
		{
			for (int i = 0; i < m_pKeyBindList->GetItemCount(); ++i)
			{
				KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
				if (!item || !item->GetString("Binding", "")[0])
					continue;
				if (item->GetInt("PrimaryKeynum", -1) == keynum)
				{
					uiSlot = "Primary";
					uiAction = item->GetString("Binding", "");
					break;
				}
				if (item->GetInt("AltKeynum", -1) == keynum)
				{
					uiSlot = "Alternate";
					uiAction = item->GetString("Binding", "");
					break;
				}
			}
			if (!strcmp(uiSlot, "unmatched") && engine[0])
			{
				KeyValues *hit = const_cast<COptionsSubKeyboard *>(this)->GetItemForBinding(engine);
				if (hit)
				{
					uiSlot = "hidden_third_plus";
					uiAction = hit->GetString("Binding", engine);
				}
				else
				{
					uiSlot = "custom_or_unmatched";
					uiAction = engine;
				}
			}
		}

		const bool engineEmpty = !engine[0];
		const bool cfgMatch =
			BindingEquals(cfg, engine) || BindingMatchesCatalog(engine, cfg) || BindingMatchesCatalog(cfg, engine);
		const bool catalogMapped =
			!strcmp(uiSlot, "Primary") || !strcmp(uiSlot, "Alternate") || !strcmp(uiSlot, "hidden_third_plus");
		const char *cls = "ENGINE_EMPTY";
		if (engineEmpty)
			cls = "ENGINE_EMPTY";
		else if (!cfgMatch)
			cls = "CONFIG_ENGINE_MISMATCH";
		else if (catalogMapped)
			cls = "CONFIG_ENGINE_MATCH+CATALOG_MAPPED";
		else
			cls = "CONFIG_ENGINE_MATCH+CUSTOM_UNMATCHED";

		CsretroMenu_CaptureLog(
			"BIND_AUDIT key=%s canon=%s keynum=%d CONFIG=\"%s\" ENGINE=\"%s\" USERCONFIG=\"%s\" UI=%s/%s class=%s",
			keyName,
			canon,
			keynum,
			cfg,
			engine,
			ucfg,
			uiAction[0] ? uiAction : "-",
			uiSlot,
			cls);
	}
	CsretroMenu_CaptureLog("BIND_AUDIT end");
}
