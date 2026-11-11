#pragma once

#include <cstdint>

// One local player identity. Pointers alias the arguments. This function
// does not copy them, does not download an image, and does not start Steam.

namespace csr
{

struct Portrait
{
    const std::uint8_t* rgba = nullptr;
    int wide = 0;
    int tall = 0;
};

struct SteamPersona
{
    bool logged_in = false;
    const char* name = nullptr;
    Portrait portrait{};
};

struct PlayerIdentity
{
    const char* name = "";
    Portrait portrait{};
};

PlayerIdentity ResolvePlayerIdentity(const char* local_name, const SteamPersona* steam);

} // namespace csr
