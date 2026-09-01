// Linux / macOS: open URL/path via xdg-open / open.
#include "system_platform.h"

#include <cstdlib>
#include <unistd.h>

void Csretro_PlatformShellOpen(const char *file)
{
	if (!file || !*file)
		return;

#if defined(__APPLE__)
	const char *bin = "open";
#else
	const char *bin = "xdg-open";
#endif

	pid_t pid = fork();
	if (pid == 0)
	{
		execlp(bin, bin, file, static_cast<char *>(nullptr));
		_exit(127);
	}
}
