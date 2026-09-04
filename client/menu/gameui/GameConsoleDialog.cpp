#include "GameConsoleDialog.h"

#include "Controls/MenuEngine.h"
#include "../vgui/window_geometry.h"

#include "KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurfaceNext.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/RichText.h"
#include "vgui_controls/TextEntry.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern bool gMenuVisible;

using namespace vgui2;

namespace
{
constexpr int kPreferredWide = 560;
constexpr int kPreferredTall = 400;
constexpr int kMinimumWide = 420;
constexpr int kMinimumTall = 240;

class CGameConsoleDialog;
CGameConsoleDialog *g_dialog = nullptr;
bool g_returnToMenu = false;
bool g_shuttingDown = false;

bool ConsoleDebugEnabled()
{
	const char *value = std::getenv("CSRETRO_CONSOLE_DEBUG");
	return value && *value && std::strcmp(value, "0") != 0;
}

void RestoreInputAfterClose();

class CConsoleEntry final : public TextEntry
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CConsoleEntry, TextEntry);

public:
	CConsoleEntry(Panel *parent, const char *name) : BaseClass(parent, name)
	{
		SetAllowNonAsciiCharacters(true);
		SendNewLine(true);
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_UP || code == KEY_DOWN)
		{
			PostMessage(GetParent(), new KeyValues("ConsoleHistory", "direction", code == KEY_UP ? -1 : 1));
			return;
		}
		if (code == KEY_ESCAPE)
		{
			PostMessage(GetParent(), new KeyValues("ConsoleClose"));
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}
};

Color ColorForCode(char code, const Color &fallback)
{
	switch (code)
	{
	case '0': return Color(216, 222, 211, 255);
	case '1': return Color(232, 84, 74, 255);
	case '2': return Color(120, 205, 112, 255);
	case '3': return Color(226, 201, 72, 255);
	case '4': return Color(100, 150, 235, 255);
	case '5': return Color(103, 205, 215, 255);
	case '6': return Color(205, 115, 205, 255);
	case '7': return Color(216, 222, 211, 255);
	case '8': return Color(170, 170, 170, 255);
	case '9': return Color(235, 145, 72, 255);
	default: return fallback;
	}
}

class CGameConsoleDialog : public Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CGameConsoleDialog, Frame);

public:
	explicit CGameConsoleDialog(Panel *parent) : BaseClass(parent, "GameConsole")
	{
		SetTitle("#GameUI_Console", true);
		SetDeleteSelfOnClose(false);
		SetSizeable(true);
		SetMoveable(true);
		SetMinimumSize(kMinimumWide, kMinimumTall);
		SetVisible(false);

		m_historyView = new RichText(this, "ConsoleHistory");
		m_historyView->SetVerticalScrollbar(true);
		m_historyView->SetMaximumCharCount(1024 * 1024);

		m_entry = new CConsoleEntry(this, "ConsoleEntry");
		m_entry->AddActionSignalTarget(this);

		m_submit = new Button(this, "ConsoleSubmit", "#GameUI_Submit", this, "submit");
	}

	void Append(const char *text)
	{
		if (!text || !*text)
			return;

		bool legacyMask = static_cast<unsigned char>(*text) == 2;
		if (static_cast<unsigned char>(*text) >= 1 && static_cast<unsigned char>(*text) <= 3)
			++text;

		Color current = m_printColor;
		m_historyView->InsertColorChange(current);
		std::string segment;
		for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
		{
			if (*p == '^' && p[1] >= '0' && p[1] <= '9')
			{
				if (!segment.empty())
				{
					m_historyView->InsertString(segment.c_str());
					segment.clear();
				}
				current = ColorForCode(static_cast<char>(p[1]), m_printColor);
				m_historyView->InsertColorChange(current);
				++p;
				continue;
			}

			unsigned char ch = legacyMask ? (*p & 0x7f) : *p;
			if (ch < 32 && ch != '\n' && ch != '\r' && ch != '\t')
				continue;
			segment.push_back(static_cast<char>(ch));
		}
		if (!segment.empty())
			m_historyView->InsertString(segment.c_str());
		m_historyView->GotoTextEnd();
	}

	void Clear()
	{
		m_historyView->SetText("");
		m_historyView->GotoTextEnd();
	}

	void ActivateConsole()
	{
		Activate();
		MoveToFront();
		m_entry->RequestFocus();
	}

protected:
	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		m_printColor = scheme->GetColor("Console.TextColor", Color(216, 222, 211, 255));
		m_historyView->SetFgColor(m_printColor);
		m_historyView->SetBgColor(scheme->GetColor("ListPanel.BgColor", Color(35, 40, 35, 205)));
		m_historyView->SetFont(scheme->GetFont("GameConsole_Mono", true));
		m_entry->SetFont(scheme->GetFont("GameConsole_Mono", true));
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		int x = 0, y = 0, w = 0, h = 0;
		GetClientArea(x, y, w, h);
		constexpr int margin = 8;
		constexpr int rowTall = 24;
		constexpr int gap = 8;
		constexpr int buttonWide = 82;
		const int contentWide = std::max(1, w - margin * 2);
		const int historyTall = std::max(1, h - margin * 2 - rowTall - gap);
		m_historyView->SetBounds(x + margin, y + margin, contentWide, historyTall);
		m_entry->SetBounds(x + margin, y + margin + historyTall + gap,
			std::max(1, contentWide - buttonWide - gap), rowTall);
		m_submit->SetBounds(x + margin + contentWide - buttonWide,
			y + margin + historyTall + gap, buttonWide, rowTall);
	}

	void OnCommand(const char *command) override
	{
		if (command && !std::strcmp(command, "submit"))
		{
			Submit();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OnClose() override
	{
		SaveGeometry();
		BaseClass::OnClose();
		if (!g_shuttingDown)
			RestoreInputAfterClose();
	}

	MESSAGE_FUNC(OnTextNewLine, "TextNewLine") { Submit(); }
	MESSAGE_FUNC(OnConsoleClose, "ConsoleClose") { GameConsole_Hide(); }
	MESSAGE_FUNC_INT(OnConsoleHistory, "ConsoleHistory", direction)
	{
		NavigateHistory(direction);
	}

private:
	void Submit()
	{
		char text[1024];
		m_entry->GetText(text, sizeof(text));
		char *begin = text;
		while (*begin && std::isspace(static_cast<unsigned char>(*begin)))
			++begin;
		char *end = begin + std::strlen(begin);
		while (end > begin && std::isspace(static_cast<unsigned char>(end[-1])))
			*--end = '\0';
		if (!*begin)
			return;

		if (m_commandHistory.empty() || m_commandHistory.back() != begin)
			m_commandHistory.emplace_back(begin);
		m_historyPosition = m_commandHistory.size();
		m_draft.clear();

		std::string echo(">");
		echo += begin;
		echo.push_back('\n');
		MenuEngine::ConsolePrint(echo.c_str());
		std::string command(begin);
		command.push_back('\n');
		MenuEngine::ClientCmdNow(command.c_str());
		m_entry->SetText("");
		m_entry->RequestFocus();
	}

	void NavigateHistory(int direction)
	{
		if (m_commandHistory.empty())
			return;
		if (m_historyPosition > m_commandHistory.size())
			m_historyPosition = m_commandHistory.size();
		if (direction < 0)
		{
			if (m_historyPosition == m_commandHistory.size())
			{
				char current[1024];
				m_entry->GetText(current, sizeof(current));
				m_draft = current;
			}
			if (m_historyPosition > 0)
				--m_historyPosition;
		}
		else if (m_historyPosition < m_commandHistory.size())
		{
			++m_historyPosition;
		}

		m_entry->SetText(m_historyPosition < m_commandHistory.size()
			? m_commandHistory[m_historyPosition].c_str() : m_draft.c_str());
		m_entry->GotoTextEnd();
	}

	void SaveGeometry()
	{
		int x = 0, y = 0, w = 0, h = 0;
		GetBounds(x, y, w, h);
		CsretroWindowGeometry::Save("Console", x, y, w, h);
	}

	RichText *m_historyView = nullptr;
	CConsoleEntry *m_entry = nullptr;
	Button *m_submit = nullptr;
	Color m_printColor{216, 222, 211, 255};
	std::vector<std::string> m_commandHistory;
	size_t m_historyPosition = 0;
	std::string m_draft;
};

void RestoreInputAfterClose()
{
	MenuEngine::EnableTextInput(false);
	if (g_returnToMenu)
	{
		gMenuVisible = true;
		MenuEngine::SetKeyDest(2); // key_menu
	}
	else
	{
		MenuEngine::SetKeyDest(1); // key_game
	}
}
} // namespace

void GameConsole_Initialize(Panel *parent)
{
	if (g_dialog || !parent)
		return;
	g_shuttingDown = false;
	g_dialog = new CGameConsoleDialog(parent);

	int sw = 640, sh = 480;
	surface()->GetScreenSize(sw, sh);
	int x = (sw - kPreferredWide) / 2;
	int y = (sh - kPreferredTall) / 2;
	int w = kPreferredWide;
	int h = kPreferredTall;
	const CsretroWindowGeometry::Bounds saved = CsretroWindowGeometry::Load("Console");
	if (saved.valid)
	{
		x = saved.x;
		y = saved.y;
		w = saved.w;
		h = saved.h;
	}
	CsretroWindowGeometry::ClampBounds(x, y, w, h, kMinimumWide, kMinimumTall, 0, 0, sw, sh);
	g_dialog->SetBounds(x, y, w, h);
}

void GameConsole_Shutdown()
{
	if (!g_dialog)
		return;
	g_shuttingDown = true;
	if (g_dialog->IsVisible())
	{
		MenuEngine::EnableTextInput(false);
		int x = 0, y = 0, w = 0, h = 0;
		g_dialog->GetBounds(x, y, w, h);
		CsretroWindowGeometry::Save("Console", x, y, w, h);
	}
	g_dialog->SetVisible(false);
	g_dialog = nullptr;
}

bool GameConsole_Toggle()
{
	if (!g_dialog)
		return false;
	if (g_dialog->IsVisible())
	{
		GameConsole_Hide();
		return true;
	}

	g_returnToMenu = gMenuVisible;
	MenuEngine::SetKeyDest(2); // key_menu routes input through VGUI2
	// The extended MenuAPI deliberately leaves SDL text input under the menu's
	// control.  Enable it only while this TextEntry owns keyboard input.
	MenuEngine::EnableTextInput(true);
	g_dialog->ActivateConsole();
	if (ConsoleDebugEnabled())
		MenuEngine::ConsolePrint("CSRETRO_CONSOLE_OPEN\n");
	return true;
}

void GameConsole_Hide()
{
	if (g_dialog && g_dialog->IsVisible())
	{
		if (ConsoleDebugEnabled())
			MenuEngine::ConsolePrint("CSRETRO_CONSOLE_CLOSE\n");
		g_dialog->Close();
	}
}

bool GameConsole_IsActive()
{
	return g_dialog && g_dialog->IsVisible();
}

void GameConsole_Print(const char *text)
{
	if (g_dialog)
		g_dialog->Append(text);
}

void GameConsole_Clear()
{
	if (g_dialog)
		g_dialog->Clear();
}
