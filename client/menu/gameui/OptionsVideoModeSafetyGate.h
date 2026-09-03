#pragma once

class COptionsDialog;

// Semi-automated Mode-Safety checks (resolution Cancel/Timeout always;
// Fullscreen/Borderless only when CSRETRO_MODE_SAFETY_ALLOW_FS=1).
// CSRETRO_MODE_SAFETY_WALLCLOCK=1: real ~10s OnTick timeout (async — poll via Poll).
void OptionsVideoModeSafety_RunGate(COptionsDialog *dialog);

// Returns true when wallclock mode finished (or not active). Call each frame from vgui_boot.
bool OptionsVideoModeSafety_Poll(COptionsDialog *dialog);

// True while waiting for real wall-clock confirm timeout (do not quit yet).
bool OptionsVideoModeSafety_IsWaitingWallclock();
