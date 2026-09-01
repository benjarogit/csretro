#pragma once

// Functional gate for COptionsSubMouse (Apply/OK/Cancel/Reset/CVars).
// Driven by CSRETRO_OPTIONS_GATE=1 — no Audio until this passes.
void OptionsMouse_RunFunctionalGate(class COptionsDialog *dialog);
