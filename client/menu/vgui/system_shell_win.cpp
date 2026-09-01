// Windows: ShellExecuteW later. Stub keeps the link green until the Win port.
#include "system_platform.h"

#ifdef _WIN32
// TODO: ShellExecuteW(NULL, L"open", …)
#endif

void Csretro_PlatformShellOpen(const char *file)
{
	(void)file;
}
