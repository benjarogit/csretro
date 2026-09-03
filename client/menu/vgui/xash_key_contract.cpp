#include "xash_key_contract.h"

namespace XashKey
{
bool IsUiCaptureable(int keynum)
{
	if (!IsValidKeynum(keynum))
		return false;

	// Joy / AUX / gamepad aliases — preserve-only in Keyboard v1.
	if (keynum >= K_JOY1 && keynum <= K_AUX32)
		return false;

	// International slots: snapshot/name/preserve; VGUI capture incomplete → not UI-editable yet.
	if (keynum >= K_INTERNATIONAL && keynum < Count())
		return false;

	// Mouse + wheel: part of classic "Keyboard" page capture set.
	if (keynum == K_MWHEELDOWN || keynum == K_MWHEELUP)
		return true;
	if (keynum >= K_MOUSE1 && keynum <= K_MOUSE5)
		return true;

	// Everything else in the printable/named keyboard range that Xash names.
	return true;
}

bool IsReserved(int keynum)
{
	return keynum == K_ESCAPE || keynum == K_START_BUTTON;
}

const char *ReservedBinding(int keynum)
{
	if (keynum == K_ESCAPE || keynum == K_START_BUTTON)
		return "cancelselect";
	return "";
}
} // namespace XashKey
