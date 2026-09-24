#include "menu_runtime_info.h"

#include "../src/menu_priv.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined(_WIN32)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace
{
constexpr const char *kBuildStamp = __DATE__ " " __TIME__;

const char *MenuGitRevision()
{
	const char *revision = getenv("CSRETRO_MENU_GIT_REV");
	return revision && *revision ? revision : "runtime-unset";
}
} // namespace

bool CsretroMenu_CaptureDebugEnabled()
{
	const char *e = getenv("CSRETRO_KEYBOARD_CAPTURE_DEBUG");
	return e && e[0] && e[0] != '0';
}

void CsretroMenu_CaptureLog(const char *fmt, ...)
{
	if (!CsretroMenu_CaptureDebugEnabled())
		return;
	char buf[768];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	// Always stderr — play.sh without -log otherwise hides Con_Printf.
	fprintf(stderr, "[CSRETRO_KB] %s\n", buf);
	fflush(stderr);
	Menu_Con("CSRETRO_KB %s", buf);
}

void CsretroMenu_LogProvenance(const char *where)
{
	const char *path = "(unknown)";
#if !defined(_WIN32)
	Dl_info info{};
	if (dladdr(reinterpret_cast<void *>(&CsretroMenu_LogProvenance), &info) && info.dli_fname)
		path = info.dli_fname;
#endif
	const char *shaEnv = getenv("CSRETRO_MENU_SHA256");
	const char *menuEnv = getenv("CSRETRO_MENU_SO");
	char msg[1024];
	snprintf(msg, sizeof(msg),
		"CSRETRO_MENU_PROVENANCE where=%s git=%s built=\"%s\" loaded=%s CSRETRO_MENU_SO=%s sha256=%s capture_debug=%d extApi=%d pid=%d",
		where ? where : "?",
		MenuGitRevision(),
		kBuildStamp,
		path,
		menuEnv && *menuEnv ? menuEnv : "(unset)",
		shaEnv && *shaEnv ? shaEnv : "(unset)",
		CsretroMenu_CaptureDebugEnabled() ? 1 : 0,
		(gExtEngReady && gExtEng.pfnEnableTextInput) ? 1 : 0,
#if !defined(_WIN32)
		static_cast<int>(getpid())
#else
		0
#endif
	);
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
	Menu_Con("%s", msg);
}
