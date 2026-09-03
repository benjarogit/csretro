#pragma once

// Central Xash key-space contract for CS Retro GameUI.
// Do not scatter magic 256/265 — always use XashKey_Count().

#include "keydefs.h"

namespace XashKey
{
// Current Xash FWGS array size (engine/client/input/in_keys.c: keys[265]).
// Extendable by engine; CS Retro reads this constant as the contract ceiling.
// Letter keynums match the raw Key_Event path: lowercase ASCII ('c'==99, not 'C'==67).
constexpr int kCount = 265;

constexpr int Count() { return kCount; }

inline bool IsValidKeynum(int keynum)
{
	return keynum >= 0 && keynum < Count();
}

// Keys the Options Keyboard UI may show in Primary/Alternate and capture.
// Joy/AUX/International: preserve-only (not UI-editable in v1).
bool IsUiCaptureable(int keynum);

// Must never be cleared/rebound by UI in a way that loses menu cancel.
bool IsReserved(int keynum);

// Engine default binding for reserved keys (may be empty).
const char *ReservedBinding(int keynum);
} // namespace XashKey
