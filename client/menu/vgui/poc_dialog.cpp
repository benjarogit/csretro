#include <cstring>

#include <vgui/IInputInternal.h>
#include <vgui/IVGui.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

#include "../src/menu_priv.h"

using namespace vgui2;

extern vgui2::IInputInternal *g_pVGuiInput;

namespace
{
class CCsretroPocFrame : public Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCsretroPocFrame, Frame);

public:
	explicit CCsretroPocFrame(Panel *parent)
		: Frame(parent, "CsretroV1Poc", false, false)
	{
		SetDeleteSelfOnClose(false);
		SetTitle("CS Retro V1 VGUI PoC", true);
		SetSizeable(true);
		SetMoveable(true);
		SetCloseButtonVisible(true);
		SetMinimizeButtonVisible(false);
		SetMaximizeButtonVisible(false);
		SetMenuButtonVisible(false);
		SetSize(420, 240);
		LoadControlSettings("resource/UI/CsretroV1Poc.res");

		m_ok = FindChildByName("OkButton", true);
		m_cancel = FindChildByName("CancelButton", true);
		m_label = FindChildByName("PocLabel", true);
		m_entry = dynamic_cast<TextEntry *>(FindChildByName("PocEntry", true));
		// Sicherstellen: ActionSignals am Frame (GetPanel/module-mismatch kann Targets auslassen).
		if (auto *ok = dynamic_cast<Button *>(m_ok))
		{
			ok->AddActionSignalTarget(this);
			ok->SetCommand("OK");
			ok->SetButtonActivationType(Button::ACTIVATE_ONPRESSED);
		}
		if (auto *cancel = dynamic_cast<Button *>(m_cancel))
		{
			cancel->AddActionSignalTarget(this);
			cancel->SetCommand("Close");
			cancel->SetButtonActivationType(Button::ACTIVATE_ONPRESSED);
		}
		if (m_entry)
		{
			m_entry->SetText("");
			m_entry->RequestFocus();
			m_lastTextLen = m_entry->GetTextLength();
		}
		MakePopup(false);
	}

	void OnCommand(const char *command) override
	{
		if (!command)
			return;
		if (!stricmp(command, "Close") || !stricmp(command, "vguicancel"))
		{
			Menu_Con("CSRETRO_V1POC_CLOSE");
			Close();
			return;
		}
		if (!stricmp(command, "OK"))
		{
			Menu_Con("CSRETRO_V1POC_CLICK_OK");
			Close();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OnClose() override
	{
		Menu_Con("CSRETRO_V1POC_CLOSE");
		BaseClass::OnClose();
	}

	void Activate() override
	{
		BaseClass::Activate();
		SetVisible(true);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		MoveToFront();
		if (m_entry)
		{
			m_entry->SetText("");
			m_lastTextLen = 0;
			m_entry->RequestFocus();
		}
		LogVisible();
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_TAB)
			Menu_Con("CSRETRO_V1POC_TAB");
		if (code == KEY_BACKSPACE)
			Menu_Con("CSRETRO_V1POC_BACKSPACE");
		if (code == KEY_ESCAPE)
		{
			Menu_Con("CSRETRO_V1POC_CLOSE");
			Close();
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

	void OnCursorMoved(int x, int y) override
	{
		BaseClass::OnCursorMoved(x, y);
		UpdateHoverFromMouse();
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		UpdateHoverFromMouse();
		UpdateTextWatch();
		RecenterIfScreenChanged();
	}

private:
	void LogVisible()
	{
		int sw = 640, sh = 480;
		surface()->GetScreenSize(sw, sh);
		m_lastSw = sw;
		m_lastSh = sh;
		Menu_Con("CSRETRO_V1POC_VISIBLE %d %d", sw, sh);
		Menu_Con("CSRETRO_V1POC_FRAME");
	}

	void RecenterIfScreenChanged()
	{
		int sw = 640, sh = 480;
		surface()->GetScreenSize(sw, sh);
		if (sw == m_lastSw && sh == m_lastSh)
			return;
		m_lastSw = sw;
		m_lastSh = sh;
		int w, h;
		GetSize(w, h);
		if (w <= 0 || h <= 0)
		{
			w = 420;
			h = 240;
			SetSize(w, h);
		}
		SetPos((sw - w) / 2, (sh - h) / 2);
		Menu_Con("CSRETRO_V1POC_RESIZE %d %d", sw, sh);
	}

	bool PanelUnderCursor(Panel *p) const
	{
		if (!p || !input())
			return false;
		return input()->GetMouseOver() == p->GetVPanel();
	}

	void UpdateHoverFromMouse()
	{
		const char *now = nullptr;
		if (PanelUnderCursor(m_ok))
			now = "OK";
		else if (PanelUnderCursor(m_cancel))
			now = "CANCEL";
		else if (PanelUnderCursor(m_entry))
			now = "ENTRY";
		else if (PanelUnderCursor(m_label))
			now = "LABEL";

		if (now && (!m_hoverTarget || strcmp(m_hoverTarget, now) != 0))
		{
			if (!strcmp(now, "OK"))
				Menu_Con("CSRETRO_V1POC_HOVER_OK");
		}
		m_hoverTarget = now;
	}

	void UpdateTextWatch()
	{
		if (!m_entry)
			return;
		const int len = m_entry->GetTextLength();
		if (len != m_lastTextLen)
		{
			m_lastTextLen = len;
			Menu_Con("CSRETRO_V1POC_TEXT %d", len);
		}
	}

	Panel *m_ok = nullptr;
	Panel *m_cancel = nullptr;
	Panel *m_label = nullptr;
	TextEntry *m_entry = nullptr;
	const char *m_hoverTarget = nullptr;
	int m_lastTextLen = -1;
	int m_lastSw = 0;
	int m_lastSh = 0;
};

CCsretroPocFrame *g_poc = nullptr;
} // namespace

bool PocDialog_Show(Panel *parent)
{
	if (!g_poc)
		g_poc = new CCsretroPocFrame(parent);
	else if (parent)
		g_poc->SetParent(parent);

	int sw = 640, sh = 480;
	surface()->GetScreenSize(sw, sh);
	int w, h;
	g_poc->GetSize(w, h);
	if (w <= 0 || h <= 0)
	{
		w = 420;
		h = 240;
		g_poc->SetSize(w, h);
	}
	g_poc->SetPos((sw - w) / 2, (sh - h) / 2);
	g_poc->Activate();
	return true;
}

void PocDialog_Hide()
{
	if (!g_poc)
		return;
	g_poc->SetVisible(false);
	g_poc->Close();
}

bool PocDialog_IsActive()
{
	return g_poc && g_poc->IsVisible();
}

void PocDialog_InjectMouse(int x, int y, bool leftDown, bool leftUp)
{
	if (!g_pVGuiInput)
		return;
	g_pVGuiInput->InternalCursorMoved(x, y);
	if (leftDown)
		g_pVGuiInput->InternalMousePressed(MOUSE_LEFT);
	if (leftUp)
		g_pVGuiInput->InternalMouseReleased(MOUSE_LEFT);
}

void PocDialog_InjectKey(int vguiKeyCode, bool down)
{
	if (!g_pVGuiInput)
		return;
	KeyCode code = static_cast<KeyCode>(vguiKeyCode);
	if (down)
	{
		g_pVGuiInput->InternalKeyCodePressed(code);
		g_pVGuiInput->InternalKeyCodeTyped(code);
	}
	else
		g_pVGuiInput->InternalKeyCodeReleased(code);
}

void PocDialog_InjectChar(wchar_t ch)
{
	if (!g_pVGuiInput)
		return;
	g_pVGuiInput->InternalKeyTyped(ch);
}
