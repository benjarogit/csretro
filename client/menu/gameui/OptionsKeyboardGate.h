#pragma once

class COptionsDialog;

void OptionsKeyboard_RunFunctionalGate(COptionsDialog *dialog);
// Physical SDL→Xash→VGuiXash_Key proof (driven by xdotool script).
// Sequence: arm F8 → (script F8) → auto-rearm → (script q while capturing) → done.
void OptionsKeyboard_ArmPhysicalCaptureProbe(COptionsDialog *dialog);
void OptionsKeyboard_PollPhysicalCaptureProbe(COptionsDialog *dialog);
bool OptionsKeyboard_PhysicalCaptureProbeArmed();
