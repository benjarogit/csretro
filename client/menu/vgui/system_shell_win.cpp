// Windows platform shell open — OPEN GATE (not a finished product path).
// Linux 3M is unblocked: Posix uses system_shell_posix.cpp (xdg-open/open).
// Before any Windows runtime gate / ship: implement ShellExecuteW (or equivalent).
// Permanent no-op stub is not acceptable in the Windows end product.
#include "system_platform.h"

#ifdef _WIN32
// TODO(windows-platform-gate): ShellExecuteW(NULL, L"open", …) — real implementation required.
#endif

void Csretro_PlatformShellOpen(const char *file)
{
	(void)file; // intentional no-op until Windows platform gate
}
