#pragma once

// Platform ShellExecute("open") backend — no fork/execlp in system_xash.cpp.
void Csretro_PlatformShellOpen(const char *file);
