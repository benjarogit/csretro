#include "menu_priv.h"

#include <cstdio>
#include <cstring>

void Profile_Defaults(ServerProfile *p)
{
	*p = ServerProfile();
}

void Profile_WriteListen(const ServerProfile *p)
{
	char buf[1024];
	snprintf(buf, sizeof(buf),
		"hostname \"%s\"\n"
		"sv_password \"%s\"\n"
		"sv_lan %d\n"
		"mp_roundtime %.1f\n"
		"mp_freezetime %.0f\n"
		"mp_friendlyfire %d\n"
		"mp_autoteambalance %d\n"
		"bot_quota %d\n"
		"bot_difficulty %d\n"
		"bot_join_team \"%s\"\n"
		"bot_enable %d\n",
		p->hostname.c_str(),
		p->password.c_str(),
		p->lan,
		p->roundtime,
		p->freezetime,
		p->friendlyfire,
		p->teambalance,
		p->bot_quota,
		p->bot_difficulty,
		p->bot_join_team.c_str(),
		p->bot_quota > 0 ? 1 : 0);

	gEng.COM_SaveFile("listenserver.cfg", buf, static_cast<int>(strlen(buf)));
}

void Profile_Start(const ServerProfile *p)
{
	Profile_WriteListen(p);
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "maxplayers %d; map %s\n", p->maxplayers, p->map.c_str());
	gEng.pfnClientCmd(0, cmd);
}
