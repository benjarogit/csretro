#include "ClassSelectPanel.h"

#include "BuySelectPanel.h"
#include "Controls/MenuEngine.h"
#include "TeamSelectPanel.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>

#include "../src/menu_priv.h"
#include "cdll_dll.h"
#include "keydefs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <strings.h>
#include <vector>

using namespace vgui2;

extern vgui2::ILocalize *g_pVGuiLocalize;

void UI_KeyEvent(int key, int down);

#ifndef KEY_DEST_GAME
#define KEY_DEST_GAME 1
#endif
#ifndef KEY_DEST_MENU
#define KEY_DEST_MENU 2
#endif

namespace
{
const char *ResForType(int menuType)
{
	return (menuType == MENU_CLASS_CT) ? "resource/UI/Classmenu_CT.res"
					  : "resource/UI/Classmenu_TER.res";
}

struct SlotBind
{
	const char *name;
	int slot;
};

const SlotBind kTerSlots[] = {
	{"terror", 1},
	{"leet", 2},
	{"arctic", 3},
	{"guerilla", 4},
	{"militia", 5},
	{"autoselect_t", 6},
	{"CancelButton", 10},
};

const SlotBind kCtSlots[] = {
	{"urban", 1},
	{"gsg9", 2},
	{"sas", 3},
	{"gign", 4},
	{"spetsnaz", 5},
	{"autoselect_ct", 6},
	{"CancelButton", 10},
};

int SlotBit(int slot)
{
	if (slot <= 0)
		return 0;
	if (slot == 10)
		return MENU_KEY_0;
	return 1 << (slot - 1);
}

const char *PreviewImageName(const char *fieldName)
{
	if (!fieldName || !fieldName[0])
		return nullptr;
	if (!strcasecmp(fieldName, "CancelButton") || !strcasecmp(fieldName, "cancelbutton"))
		return nullptr;
	if (!strcasecmp(fieldName, "autoselect_t") || !strcasecmp(fieldName, "militia"))
		return "t_random";
	if (!strcasecmp(fieldName, "autoselect_ct") || !strcasecmp(fieldName, "spetsnaz"))
		return "ct_random";
	return fieldName;
}

class CClassSelectPanel;

class CClassHoverButton : public Button
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CClassHoverButton, Button);

public:
	CClassHoverButton(Panel *parent, const char *name)
		: BaseClass(parent, name, "")
	{
	}

	void SetHost(CClassSelectPanel *host) { m_host = host; }
	void SetPreviewName(const char *name) { m_preview = name ? name : ""; }

	void OnCursorEntered() override;

private:
	CClassSelectPanel *m_host = nullptr;
	std::string m_preview;
};

class CClassSelectPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CClassSelectPanel, EditablePanel);

public:
	CClassSelectPanel(Panel *parent, int menuType)
		: BaseClass(parent, "ClassMenu")
		, m_type(menuType)
	{
		SetProportional(true);
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		if (scheme())
		{
			const HScheme client = scheme()->GetScheme("ClientScheme");
			if (client)
				SetScheme(client);
		}
		LoadControlSettings(ResForType(menuType));
		ApplyClassLabels();
		StyleButtons();
		BindHover();
	}

	bool HasClassButtons()
	{
		if (m_type == MENU_CLASS_CT)
			return FindChildByName("urban") && FindChildByName("gsg9");
		return FindChildByName("terror") && FindChildByName("leet");
	}

	void Open(int validSlots)
	{
		m_slots = validSlots;
		if (m_slots == 0)
			m_slots = MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_3 | MENU_KEY_4 | MENU_KEY_5;
		ApplySlots();
		SetVisible(true);
		MoveToFront();
		RequestFocus();
		ShowDefaultPreview();
		LogOpen();
	}

	void ApplySlots()
	{
		const bool cz = (m_slots & MENU_KEY_6) != 0;
		const SlotBind *binds = Binds();
		const int n = BindCount();
		for (int i = 0; i < n; ++i)
		{
			const SlotBind &bind = binds[i];
			Panel *child = FindChildByName(bind.name);
			if (!child)
				continue;
			if (bind.slot == 10)
			{
				child->SetVisible(true);
				continue;
			}
			if (!cz && (bind.slot == 5) && IsSkin5(bind.name))
			{
				child->SetVisible(false);
				continue;
			}
			int slot = bind.slot;
			if (!cz && IsAutoselect(bind.name))
			{
				slot = 5;
				if (auto *btn = dynamic_cast<Button *>(child))
					btn->SetCommand("joinclass 5");
			}
			child->SetVisible((m_slots & SlotBit(slot)) != 0);
		}
		RelayoutVisibleButtons();
	}

	bool ActivateSlot(int slot)
	{
		Button *btn = ButtonForSlot(slot);
		if (!btn || !btn->IsVisible() || !btn->IsEnabled())
			return false;
		btn->DoClick();
		return true;
	}

	int VisibleButtonCount()
	{
		int n = 0;
		const SlotBind *binds = Binds();
		const int count = BindCount();
		for (int i = 0; i < count; ++i)
		{
			Panel *child = FindChildByName(binds[i].name);
			if (child && child->IsVisible())
				++n;
		}
		return n;
	}

	int SlotMask() const { return m_slots; }
	int MenuType() const { return m_type; }

	bool LabelLooksLocalized(const char *name)
	{
		auto *label = dynamic_cast<Label *>(FindChildByName(name));
		if (!label)
			return false;
		wchar_t text[128] = {};
		label->GetText(text, sizeof(text));
		if (text[0] == L'\0' || text[0] == L'#')
			return false;
		return wcsstr(text, L"Cstrike_") == nullptr;
	}

	// Roh-Token sichtbar = Fehler. Leerer Text ist erlaubt (Steam hat
	// #Cstrike_Class_Info in der .res, aber nicht in cstrike_english).
	bool LabelIsRawToken(const char *name)
	{
		auto *label = dynamic_cast<Label *>(FindChildByName(name));
		if (!label)
			return false;
		wchar_t text[128] = {};
		label->GetText(text, sizeof(text));
		if (text[0] == L'\0')
			return false;
		return text[0] == L'#' || wcsstr(text, L"Cstrike_") != nullptr;
	}

	bool HasPreview() const { return m_preview && m_preview->IsVisible() && m_preview->GetImage(); }

	void ShowClassPreview(const char *imageName)
	{
		if (!imageName || !imageName[0] || !m_preview)
			return;
		char path[96];
		snprintf(path, sizeof(path), "gfx/vgui/%s", imageName);
		m_preview->SetImage(path);
		if (Panel *info = FindChildByName("ClassInfo"))
		{
			info->SetVisible(true);
			info->SetEnabled(true);
		}
		if (auto *caption = dynamic_cast<Label *>(FindChildByName("classInfoLabel")))
		{
			wchar_t text[128] = {};
			caption->GetText(text, sizeof(text));
			const bool raw = text[0] == L'#' || wcsstr(text, L"Cstrike_") != nullptr;
			caption->SetVisible(text[0] != L'\0' && !raw);
		}
		m_preview->SetVisible(true);
	}

	Panel *CreateControlByName(const char *controlName) override
	{
		if (controlName && !strcasecmp(controlName, "MouseOverPanelButton"))
			return new CClassHoverButton(nullptr, nullptr);
		return BaseClass::CreateControlByName(controlName);
	}

	void OnCommand(const char *command) override
	{
		if (!command || !command[0])
			return;
		if (!strcasecmp(command, "vguicancel"))
		{
			Menu_Con("CSRETRO_CLASS_CMD vguicancel");
			ClassSelect_Hide();
			return;
		}
		if (!strncasecmp(command, "joinclass ", 10))
		{
			Menu_Con("CSRETRO_CLASS_CMD %s", command);
			char buf[64];
			snprintf(buf, sizeof(buf), "%s\n", command);
			MenuEngine::ClientCmdNow(buf);
			ClassSelect_Hide();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_ESCAPE)
		{
			ClassSelect_Hide();
			return;
		}
		if (code == KEY_0)
		{
			ActivateSlot(10);
			return;
		}
		if (code >= KEY_1 && code <= KEY_9)
		{
			ActivateSlot(code - KEY_0);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

private:
	int m_type = MENU_CLASS_T;
	int m_slots = 0;
	ImagePanel *m_preview = nullptr;

	const SlotBind *Binds() const
	{
		return (m_type == MENU_CLASS_CT) ? kCtSlots : kTerSlots;
	}

	int BindCount() const
	{
		return (m_type == MENU_CLASS_CT)
			? static_cast<int>(sizeof(kCtSlots) / sizeof(kCtSlots[0]))
			: static_cast<int>(sizeof(kTerSlots) / sizeof(kTerSlots[0]));
	}

	static bool IsSkin5(const char *name)
	{
		return name && (!strcasecmp(name, "militia") || !strcasecmp(name, "spetsnaz"));
	}

	static bool IsAutoselect(const char *name)
	{
		return name && (!strcasecmp(name, "autoselect_t") || !strcasecmp(name, "autoselect_ct"));
	}

	int EffectiveSlot(const SlotBind &bind) const
	{
		const bool cz = (m_slots & MENU_KEY_6) != 0;
		if (!cz && IsAutoselect(bind.name))
			return 5;
		return bind.slot;
	}

	Button *ButtonForSlot(int slot)
	{
		const SlotBind *binds = Binds();
		const int n = BindCount();
		for (int i = 0; i < n; ++i)
		{
			if (EffectiveSlot(binds[i]) != slot)
				continue;
			return dynamic_cast<Button *>(FindChildByName(binds[i].name));
		}
		return nullptr;
	}

	void ApplyClassLabels()
	{
		// Steam-Classmenu setzt bei mehreren Buttons labelText zweimal
		// (erst "", dann #Token). KeyValues::GetString nimmt den ersten —
		// ohne Nachzug bleiben L337/Arctic/… leer statt lokalisiert.
		struct LabelFix
		{
			const char *name;
			const char *token;
		};
		const LabelFix ter[] = {
			{"joinClass", "#Cstrike_Join_Class"},
			{"terror", "#Cstrike_Terror"},
			{"leet", "#Cstrike_L337_Krew"},
			{"arctic", "#Cstrike_Arctic"},
			{"guerilla", "#Cstrike_Guerilla"},
			{"militia", "#Cstrike_Militia"},
			{"autoselect_t", "#Cstrike_Auto_Select"},
			{"CancelButton", "#Cstrike_Cancel"},
			{"classInfoLabel", "#Cstrike_Class_Info"},
		};
		const LabelFix ct[] = {
			{"joinClass", "#Cstrike_Join_Class"},
			{"urban", "#Cstrike_Urban"},
			{"gsg9", "#Cstrike_GSG9"},
			{"sas", "#Cstrike_SAS"},
			{"gign", "#Cstrike_GIGN"},
			{"spetsnaz", "#Cstrike_Spetsnaz"},
			{"autoselect_ct", "#Cstrike_Auto_Select"},
			{"CancelButton", "#Cstrike_Cancel"},
			{"classInfoLabel", "#Cstrike_Class_Info"},
		};
		const LabelFix *fixes = (m_type == MENU_CLASS_CT) ? ct : ter;
		const int n = (m_type == MENU_CLASS_CT)
			? static_cast<int>(sizeof(ct) / sizeof(ct[0]))
			: static_cast<int>(sizeof(ter) / sizeof(ter[0]));
		for (int i = 0; i < n; ++i)
		{
			auto *label = dynamic_cast<Label *>(FindChildByName(fixes[i].name));
			if (!label)
				continue;
			label->SetText(fixes[i].token);
			if (strcasecmp(fixes[i].name, "classInfoLabel") != 0)
				continue;
			wchar_t text[128] = {};
			label->GetText(text, sizeof(text));
			if (text[0] == L'#' || wcsstr(text, L"Cstrike_") != nullptr)
				label->SetText("");
		}
	}

	void StyleButtons()
	{
		IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
		const Color fg = GetSchemeColor("BrightControlText", Color(255, 176, 0, 255), sch);
		const Color armed = GetSchemeColor("BrightBaseText", Color(255, 220, 80, 255), sch);
		const Color bg(0, 0, 0, 0);
		const Color armedBg = GetSchemeColor("SelectionBG", Color(255, 176, 0, 100), sch);

		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn)
				continue;
			btn->SetPaintBackgroundEnabled(true);
			btn->SetDefaultColor(fg, bg);
			btn->SetArmedColor(armed, armedBg);
			btn->SetDepressedColor(armed, armedBg);
			btn->SetDefaultBorder(nullptr);
			btn->SetDepressedBorder(nullptr);
			btn->SetKeyFocusBorder(nullptr);
			btn->SetContentAlignment(Label::a_west);
			btn->SetButtonActivationType(Button::ACTIVATE_ONPRESSED);
		}

		if (auto *title = dynamic_cast<Label *>(FindChildByName("joinClass")))
			title->SetFgColor(fg);
		if (auto *info = dynamic_cast<Label *>(FindChildByName("classInfoLabel")))
			info->SetFgColor(fg);
	}

	void BindHover()
	{
		Panel *info = FindChildByName("ClassInfo");
		if (info)
		{
			info->SetPaintBackgroundEnabled(true);
			info->SetMouseInputEnabled(false);
			IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
			info->SetBgColor(GetSchemeColor("WindowBG", Color(0, 0, 0, 200), sch));
			int w = 0, h = 0;
			info->GetSize(w, h);
			m_preview = new ImagePanel(info, "ClassPreview");
			int pad = 8;
			if (IsProportional() && scheme())
				pad = scheme()->GetProportionalScaledValue(8);
			int iw = w > pad * 2 ? w - pad * 2 : w;
			int maxSide = 256;
			if (IsProportional() && scheme())
				maxSide = scheme()->GetProportionalScaledValue(256);
			if (iw > maxSide)
				iw = maxSide;
			m_preview->SetBounds(pad, pad, iw, iw);
			m_preview->SetShouldScaleImage(true);
			m_preview->SetVisible(false);
		}

		const SlotBind *binds = Binds();
		const int n = BindCount();
		for (int i = 0; i < n; ++i)
		{
			auto *btn = dynamic_cast<CClassHoverButton *>(FindChildByName(binds[i].name));
			if (!btn)
				continue;
			btn->SetHost(this);
			if (const char *preview = PreviewImageName(binds[i].name))
				btn->SetPreviewName(preview);
		}
	}

	void ShowDefaultPreview()
	{
		const SlotBind *binds = Binds();
		const int n = BindCount();
		for (int i = 0; i < n; ++i)
		{
			Panel *child = FindChildByName(binds[i].name);
			if (!child || !child->IsVisible() || binds[i].slot == 10)
				continue;
			if (const char *preview = PreviewImageName(binds[i].name))
			{
				ShowClassPreview(preview);
				return;
			}
		}
	}

	void RelayoutVisibleButtons()
	{
		std::vector<Panel *> visible;
		visible.reserve(8);
		const SlotBind *binds = Binds();
		const int n = BindCount();
		for (int i = 0; i < n; ++i)
		{
			Panel *child = FindChildByName(binds[i].name);
			if (child && child->IsVisible())
				visible.push_back(child);
		}
		if (visible.empty())
			return;
		int x = 0, y = 0, w = 0, h = 0;
		visible[0]->GetBounds(x, y, w, h);
		int gap = 12;
		if (IsProportional() && scheme())
			gap = scheme()->GetProportionalScaledValue(12);
		const int step = h > 0 ? h + gap : 32;
		for (size_t i = 0; i < visible.size(); ++i)
		{
			int cx = 0, cy = 0, cw = 0, ch = 0;
			visible[i]->GetBounds(cx, cy, cw, ch);
			visible[i]->SetPos(cx, y + static_cast<int>(i) * step);
		}
	}

	void LogOpen()
	{
		int skin5 = 0, autoselect = 0, cancel = 0;
		if (Panel *p = FindChildByName(m_type == MENU_CLASS_CT ? "spetsnaz" : "militia"))
			skin5 = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName(m_type == MENU_CLASS_CT ? "autoselect_ct" : "autoselect_t"))
			autoselect = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName("CancelButton"))
			cancel = p->IsVisible() ? 1 : 0;
		Menu_Con("CSRetro-VGUI: %s (%d)", ResForType(m_type), m_type);
		Menu_Con("CSRETRO_CLASS_VGUI open type=%d slots=%d buttons=%d skin5=%d auto=%d cancel=%d",
			m_type, m_slots, VisibleButtonCount(), skin5, autoselect, cancel);
	}
};

void CClassHoverButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	if (m_host && !m_preview.empty())
		m_host->ShowClassPreview(m_preview.c_str());
}

class CClassSelectOverlay : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CClassSelectOverlay, Panel);

public:
	explicit CClassSelectOverlay(Panel *parent)
		: BaseClass(parent, "ClassSelectOverlay")
	{
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		if (scheme())
		{
			const HScheme client = scheme()->GetScheme("ClientScheme");
			if (client)
				SetScheme(client);
		}
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetBgColor(GetSchemeColor("ViewportBG", Color(0, 0, 0, 200), pScheme));
	}

	void PerformLayout() override
	{
		if (Panel *p = GetParent())
		{
			int w = 0, h = 0;
			p->GetSize(w, h);
			SetBounds(0, 0, w, h);
		}
		BaseClass::PerformLayout();
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (m_cls)
		{
			m_cls->OnKeyCodeTyped(code);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

	CClassSelectPanel *m_cls = nullptr;
};

CClassSelectOverlay *g_overlay = nullptr;
CClassSelectPanel *g_panel = nullptr;
Panel *g_host = nullptr;
bool g_keyDestPushed = false;
} // namespace

static void DestroyClassUi()
{
	delete g_overlay;
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
}

bool ClassSelect_Show(Panel *root, int menuType, int validSlots)
{
	if (menuType != MENU_CLASS_T && menuType != MENU_CLASS_CT)
		return false;

	if (g_panel && g_panel->MenuType() != menuType)
		DestroyClassUi();

	if (!g_overlay)
	{
		if (!root)
			return false;
		g_overlay = new CClassSelectOverlay(root);
		g_panel = new CClassSelectPanel(g_overlay, menuType);
		g_overlay->m_cls = g_panel;
		g_host = root;
		if (!g_panel->HasClassButtons())
		{
			Menu_Con("CSRETRO_CLASS_VGUI fail %s", ResForType(menuType));
			DestroyClassUi();
			return false;
		}
	}
	else if (root)
	{
		g_overlay->SetParent(root);
		g_host = root;
	}

	Panel *host = g_overlay->GetParent();
	if (!host)
		host = g_host;
	if (!host)
		return false;
	if (g_overlay->GetParent() != host)
		g_overlay->SetParent(host);
	int w = 0, h = 0;
	host->GetSize(w, h);
	g_overlay->SetBounds(0, 0, w, h);
	g_overlay->SetVisible(true);
	g_overlay->MoveToFront();
	g_panel->Open(validSlots);
	g_overlay->RequestFocus();
	if (gEng.pfnSetKeyDest)
	{
		gEng.pfnSetKeyDest(KEY_DEST_MENU);
		g_keyDestPushed = true;
	}
	return true;
}

void ClassSelect_Hide()
{
	if (g_overlay && g_overlay->IsVisible())
		Menu_Con("CSRETRO_CLASS_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
	if (g_overlay)
		g_overlay->SetVisible(false);
	if (g_keyDestPushed && !gMenuVisible && !TeamSelect_IsActive() && !BuySelect_IsActive())
	{
		if (gEng.pfnSetKeyDest)
			gEng.pfnSetKeyDest(KEY_DEST_GAME);
		g_keyDestPushed = false;
	}
}

bool ClassSelect_IsActive()
{
	return g_overlay && g_overlay->IsVisible() && g_panel && g_panel->IsVisible();
}

bool ClassSelect_ActivateSlot(int slot)
{
	return g_panel && g_panel->IsVisible() && g_panel->ActivateSlot(slot);
}

int ClassSelect_MenuType()
{
	return g_panel ? g_panel->MenuType() : 0;
}

void ClassSelect_Shutdown()
{
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
	g_keyDestPushed = false;
}

void ClassSelect_GateTick()
{
	if (!getenv("CSRETRO_CLASS_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	if (step >= 99)
		return;

	auto failDone = [](const char *why) {
		Menu_Con("CSRETRO_CLASS_GATE_FAIL %s", why);
		Menu_Con("CSRETRO_CLASS_GATE_DONE");
		step = 99;
	};

	if (step == 0)
	{
		if (!TeamSelect_IsActive())
			return;
		++hold;
		if (hold < 20)
			return;
		UI_KeyEvent('1', 1);
		UI_KeyEvent('1', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 1)
	{
		if (!ClassSelect_IsActive() || !g_panel || g_panel->MenuType() != MENU_CLASS_T)
			return;
		++hold;
		if (hold < 45)
			return;
		const int title = g_panel->LabelLooksLocalized("joinClass") ? 1 : 0;
		const int terror = g_panel->LabelLooksLocalized("terror") ? 1 : 0;
		const int leet = g_panel->LabelLooksLocalized("leet") ? 1 : 0;
		const int arctic = g_panel->LabelLooksLocalized("arctic") ? 1 : 0;
		const int guerilla = g_panel->LabelLooksLocalized("guerilla") ? 1 : 0;
		const int autoselect = g_panel->LabelLooksLocalized("autoselect_t") ? 1 : 0;
		const int cancel = g_panel->LabelLooksLocalized("CancelButton") ? 1 : 0;
		const int classinfo = g_panel->LabelIsRawToken("classInfoLabel") ? 0 : 1;
		int militia = 0;
		if (Panel *p = g_panel->FindChildByName("militia"))
			militia = p->IsVisible() ? 1 : 0;
		Menu_Con("CSRETRO_CLASS_GATE_OPEN type=%d visible=1 buttons=%d slots=%d title=%d "
			 "terror=%d leet=%d arctic=%d guerilla=%d auto=%d cancel=%d militia=%d preview=%d classinfo=%d",
			g_panel->MenuType(), g_panel->VisibleButtonCount(), g_panel->SlotMask(),
			title, terror, leet, arctic, guerilla, autoselect, cancel, militia,
			g_panel->HasPreview() ? 1 : 0, classinfo);
		if (g_pVGuiLocalize)
		{
			const char *probes[] = {
				"Cstrike_Join_Class",
				"Cstrike_Terror",
				"Cstrike_L337_Krew",
				"Cstrike_Arctic",
				"Cstrike_Guerilla",
				"Cstrike_Auto_Select",
				"Cstrike_Cancel",
				"Cstrike_Urban",
				"Cstrike_GSG9",
				"Cstrike_SAS",
				"Cstrike_GIGN",
			};
			for (const char *tok : probes)
			{
				wchar_t *w = g_pVGuiLocalize->Find(tok);
				if (!w || !w[0])
					Menu_Con("CSRETRO_LOC_MISSING %s", tok);
			}
		}
		MenuEngine::ClientCmd("screenshot\n");
		++step;
		hold = 0;
		return;
	}

	if (step == 2)
	{
		++hold;
		if (hold < 20)
			return;
		UI_KeyEvent(K_ESCAPE, 1);
		UI_KeyEvent(K_ESCAPE, 0);
		Menu_Con("CSRETRO_CLASS_GATE_ESC visible=%d", ClassSelect_IsActive() ? 1 : 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 3)
	{
		if (!g_overlay || !g_panel)
		{
			failDone("panel weg");
			return;
		}
		Panel *root = g_overlay->GetParent();
		if (!ClassSelect_Show(root, MENU_CLASS_T, g_panel->SlotMask()))
		{
			failDone("reopen");
			return;
		}
		++step;
		hold = 0;
		return;
	}

	if (step == 4)
	{
		++hold;
		if (hold < 15)
			return;
		UI_KeyEvent('1', 1);
		UI_KeyEvent('1', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 5)
	{
		++hold;
		if (hold < 20)
			return;
		Menu_Con("CSRETRO_CLASS_GATE_JOIN visible=%d", ClassSelect_IsActive() ? 1 : 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 6)
	{
		++hold;
		if (hold < 90)
			return;
		MenuEngine::ClientCmdNow("jointeam 2\n");
		Menu_Con("CSRETRO_CLASS_GATE_CT_REQUEST");
		++step;
		hold = 0;
		return;
	}

	if (step == 7)
	{
		++hold;
		if (ClassSelect_IsActive() && g_panel && g_panel->MenuType() == MENU_CLASS_CT)
		{
			if (hold < 30)
				return;
			const int title = g_panel->LabelLooksLocalized("joinClass") ? 1 : 0;
			const int urban = g_panel->LabelLooksLocalized("urban") ? 1 : 0;
			const int gsg9 = g_panel->LabelLooksLocalized("gsg9") ? 1 : 0;
			const int sas = g_panel->LabelLooksLocalized("sas") ? 1 : 0;
			const int gign = g_panel->LabelLooksLocalized("gign") ? 1 : 0;
			const int autoselect = g_panel->LabelLooksLocalized("autoselect_ct") ? 1 : 0;
			int spetsnaz = 0;
			if (Panel *p = g_panel->FindChildByName("spetsnaz"))
				spetsnaz = p->IsVisible() ? 1 : 0;
			Menu_Con("CSRETRO_CLASS_GATE_CT type=%d visible=1 title=%d urban=%d gsg9=%d sas=%d "
				 "gign=%d auto=%d spetsnaz=%d",
				g_panel->MenuType(), title, urban, gsg9, sas, gign, autoselect, spetsnaz);
			UI_KeyEvent('1', 1);
			UI_KeyEvent('1', 0);
			++step;
			hold = 0;
			return;
		}
		if (hold > 180)
		{
			failDone("ct menu fehlt");
			return;
		}
		return;
	}

	if (step == 8)
	{
		++hold;
		if (hold < 20)
			return;
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_CLASS_GATE_DONE");
		step = 99;
	}
}
