#pragma once

namespace vgui2
{
class Panel;
}

// CS Retro's windowed VGUI2 developer console. The engine remains the source
// of truth for commands and scrollback; this module owns presentation/input.
void GameConsole_Initialize(vgui2::Panel *parent);
void GameConsole_Shutdown();
bool GameConsole_Toggle();
void GameConsole_Hide();
bool GameConsole_IsActive();
void GameConsole_FocusEntry();
void GameConsole_FeedKey(int vguiKeyCode, bool down);
void GameConsole_FeedChar(wchar_t ch);
void GameConsole_HandleRawKey(int xashKey, bool down);
void GameConsole_Print(const char *text);
void GameConsole_Clear();

