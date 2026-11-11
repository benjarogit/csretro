// The product process does not load Valve's i386 libsteam_api.so.
// Steam accessors used by this tree live in the tier0 host library.
extern "C" void csretro_steam_api_host_stub() {}
