#pragma once

// Classic Preferred / Reference Size for COptionsDialog (Golden 5971 / CKF / OpenGoldSrc).
// Not a hard maximum: later content-driven grow, responsive desktop, and HiDPI are separate layers.
namespace CsretroOptionsClassic
{
constexpr int kPreferredWide = 512;
constexpr int kPreferredTall = 406;
// Keyboard .res at Classic Preferred — never derive these from a 64×24 parent.
constexpr int kKeyboardListX = 8;
constexpr int kKeyboardListY = 10;
constexpr int kKeyboardListW = 480;
constexpr int kKeyboardListH = 258;
constexpr int kKeyboardDefaultsX = 8;
constexpr int kKeyboardDefaultsY = 276;
constexpr int kKeyboardDefaultsW = 134;
constexpr int kKeyboardDefaultsH = 24;
constexpr int kKeyboardChangeX = 272;
constexpr int kKeyboardChangeY = 276;
constexpr int kKeyboardChangeW = 106;
constexpr int kKeyboardChangeH = 24;
constexpr int kKeyboardClearX = 384;
constexpr int kKeyboardClearY = 276;
constexpr int kKeyboardClearW = 105;
constexpr int kKeyboardClearH = 24;
// Keyboard list floor when shrinking — not the dialog minimum.
constexpr int kKeyboardListMinW = 280;
constexpr int kKeyboardListMinH = 120;
}
