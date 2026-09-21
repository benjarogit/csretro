#include "menu_priv.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
void AppendLine(std::string &cfg, const char *fmt, ...) CSRETRO_PRINTF_LIKE(2, 3);
void AppendLine(std::string &cfg, const char *fmt, ...)
{
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	cfg += line;
}
} // namespace

void Profile_Defaults(ServerProfile *p)
{
	*p = ServerProfile();
}

void Profile_WriteListen(const ServerProfile *p)
{
	std::string cfg;

	// Serveridentität und Botblock: feste Felder, weil Profile_Start sie strukturell braucht.
	AppendLine(cfg, "hostname \"%s\"\n", p->hostname.c_str());
	AppendLine(cfg, "sv_password \"%s\"\n", p->password.c_str());
	AppendLine(cfg, "sv_lan %d\n", p->lan);
	AppendLine(cfg, "bot_quota %d\n", p->bot_quota);
	AppendLine(cfg, "bot_difficulty %d\n", p->bot_difficulty);
	AppendLine(cfg, "bot_join_team \"%s\"\n", p->bot_join_team.c_str());
	AppendLine(cfg, "bot_enable %d\n", p->bot_quota > 0 ? 1 : 0);
	// Listen (maxclients>1) drosselt den Host auf cl_updaterate; 60 war die Xash-Kappe.
	AppendLine(cfg, "sv_maxupdaterate 102\n");

	// Gameplay-Regeln so, wie die Settings-Listen sie aus settings.scr gelesen haben.
	for (const auto &kv : p->gameplay)
		AppendLine(cfg, "%s \"%s\"\n", kv.first.c_str(), kv.second.c_str());

	gEng.COM_SaveFile("listenserver.cfg", cfg.c_str(), static_cast<int>(cfg.size()));
}

void Profile_Start(const ServerProfile *p)
{
	Profile_WriteListen(p);
	// listenserver.cfg must be exec'd before map — Xash does not auto-exec
	// lservercfgfile (only dedicated servercfgfile). Without this, GameDLL
	// keeps stock defaults (mp_freezetime 15, startmoney 800, …).
	char execCmd[128];
	snprintf(execCmd, sizeof(execCmd), "exec listenserver.cfg\n");
	if (gEng.pfnClientCmd)
		gEng.pfnClientCmd(1, execCmd);

	char cmd[256];
	snprintf(cmd, sizeof(cmd), "maxplayers %d; map %s\n", p->maxplayers, p->map.c_str());
	if (gEng.pfnClientCmd)
		gEng.pfnClientCmd(0, cmd);
}
