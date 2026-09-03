// Xash keydefs → vgui2::KeyCode. Replaces Win32 vgui_key_translation.cpp.
#include "vgui_key_translation.h"

#include <cstring>

#include "tier0/dbg.h"
#include "vgui/KeyCode.h"

#include "keydefs.h"

static vgui2::KeyCode s_xashToVgui[512];
static int s_vguiToXash[vgui2::KEY_CODE_COUNT];

void KeyCode_InitKeyTranslationTable()
{
	memset(s_xashToVgui, 0, sizeof(s_xashToVgui));
	memset(s_vguiToXash, 0, sizeof(s_vguiToXash));

	auto map = [](int xash, vgui2::KeyCode code) {
		if (xash >= 0 && xash < 512)
			s_xashToVgui[xash] = code;
		if (code > vgui2::KEY_NONE && code < vgui2::KEY_CODE_COUNT)
			s_vguiToXash[code] = xash;
	};

	for (int c = '0'; c <= '9'; ++c)
		map(c, static_cast<vgui2::KeyCode>(vgui2::KEY_0 + (c - '0')));
	for (int c = 'a'; c <= 'z'; ++c)
		map(c, static_cast<vgui2::KeyCode>(vgui2::KEY_A + (c - 'a')));
	for (int c = 'A'; c <= 'Z'; ++c)
		map(c, static_cast<vgui2::KeyCode>(vgui2::KEY_A + (c - 'A')));

	map(K_TAB, vgui2::KEY_TAB);
	map(K_ENTER, vgui2::KEY_ENTER);
	map(K_ESCAPE, vgui2::KEY_ESCAPE);
	map(K_SPACE, vgui2::KEY_SPACE);
	map(K_BACKSPACE, vgui2::KEY_BACKSPACE);
	map(K_UPARROW, vgui2::KEY_UP);
	map(K_DOWNARROW, vgui2::KEY_DOWN);
	map(K_LEFTARROW, vgui2::KEY_LEFT);
	map(K_RIGHTARROW, vgui2::KEY_RIGHT);
	map(K_ALT, vgui2::KEY_LALT);
	map(K_CTRL, vgui2::KEY_LCONTROL);
	map(K_SHIFT, vgui2::KEY_LSHIFT);
	// Xash has one ALT/CTRL/SHIFT slot — both VGUI sides map to the same keynum.
	s_vguiToXash[vgui2::KEY_RALT] = K_ALT;
	s_vguiToXash[vgui2::KEY_RCONTROL] = K_CTRL;
	s_vguiToXash[vgui2::KEY_RSHIFT] = K_SHIFT;
	map(K_CAPSLOCK, vgui2::KEY_CAPSLOCK);
	map(K_SCROLLLOCK, vgui2::KEY_SCROLLLOCK);
	map(K_INS, vgui2::KEY_INSERT);
	map(K_DEL, vgui2::KEY_DELETE);
	map(K_HOME, vgui2::KEY_HOME);
	map(K_END, vgui2::KEY_END);
	map(K_PGUP, vgui2::KEY_PAGEUP);
	map(K_PGDN, vgui2::KEY_PAGEDOWN);
	map(K_PAUSE, vgui2::KEY_BREAK);
	map(K_WIN, vgui2::KEY_LWIN);

	map(K_F1, vgui2::KEY_F1);
	map(K_F2, vgui2::KEY_F2);
	map(K_F3, vgui2::KEY_F3);
	map(K_F4, vgui2::KEY_F4);
	map(K_F5, vgui2::KEY_F5);
	map(K_F6, vgui2::KEY_F6);
	map(K_F7, vgui2::KEY_F7);
	map(K_F8, vgui2::KEY_F8);
	map(K_F9, vgui2::KEY_F9);
	map(K_F10, vgui2::KEY_F10);
	map(K_F11, vgui2::KEY_F11);
	map(K_F12, vgui2::KEY_F12);

	map(K_KP_ENTER, vgui2::KEY_PAD_ENTER);
	map(K_KP_SLASH, vgui2::KEY_PAD_DIVIDE);
	map(K_KP_MUL, vgui2::KEY_PAD_MULTIPLY);
	map(K_KP_MINUS, vgui2::KEY_PAD_MINUS);
	map(K_KP_PLUS, vgui2::KEY_PAD_PLUS);
	map(K_KP_DEL, vgui2::KEY_PAD_DECIMAL);
	map(K_KP_INS, vgui2::KEY_PAD_0);
	map(K_KP_END, vgui2::KEY_PAD_1);
	map(K_KP_DOWNARROW, vgui2::KEY_PAD_2);
	map(K_KP_PGDN, vgui2::KEY_PAD_3);
	map(K_KP_LEFTARROW, vgui2::KEY_PAD_4);
	map(K_KP_5, vgui2::KEY_PAD_5);
	map(K_KP_RIGHTARROW, vgui2::KEY_PAD_6);
	map(K_KP_HOME, vgui2::KEY_PAD_7);
	map(K_KP_UPARROW, vgui2::KEY_PAD_8);
	map(K_KP_PGUP, vgui2::KEY_PAD_9);
	map(K_KP_NUMLOCK, vgui2::KEY_NUMLOCK);

	// Punctuation (ASCII)
	map('-', vgui2::KEY_MINUS);
	map('=', vgui2::KEY_EQUAL);
	map('[', vgui2::KEY_LBRACKET);
	map(']', vgui2::KEY_RBRACKET);
	map('\\', vgui2::KEY_BACKSLASH);
	map(';', vgui2::KEY_SEMICOLON);
	map('\'', vgui2::KEY_APOSTROPHE);
	map('`', vgui2::KEY_BACKQUOTE);
	map(',', vgui2::KEY_COMMA);
	map('.', vgui2::KEY_PERIOD);
	map('/', vgui2::KEY_SLASH);
}

vgui2::KeyCode KeyCode_VirtualKeyToVGUI(int key)
{
	if (key < 0 || key >= 512)
		return vgui2::KEY_NONE;
	return s_xashToVgui[key];
}

int KeyCode_VGUIToVirtualKey(vgui2::KeyCode code)
{
	if (code <= vgui2::KEY_NONE || code >= vgui2::KEY_CODE_COUNT)
		return 0;
	return s_vguiToXash[code];
}
