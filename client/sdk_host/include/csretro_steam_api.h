#pragma once

#include "player_identity/Resolve.h"

/*
 * The one ELF64 libsteam_api.so. It does not load Valve's i386 library, does
 * not start Steam, and does not store credentials. A name it does not know is
 * a null pointer and is not called.
 */

extern "C" void* csr_steam_proxy_interface(const char* name);
extern "C" void csr_steam_proxy_persona(csr::SteamPersona* persona);
