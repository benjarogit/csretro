#include "player_identity/Resolve.h"

namespace csr
{
namespace
{

bool HasText(const char* value)
{
    return value != nullptr && value[0] != '\0';
}

bool HasPortrait(const Portrait& portrait)
{
    return portrait.rgba != nullptr && portrait.wide > 0 && portrait.tall > 0;
}

} // namespace

PlayerIdentity ResolvePlayerIdentity(const char* local_name, const SteamPersona* steam)
{
    PlayerIdentity identity;
    identity.name = local_name != nullptr ? local_name : "";

    if (steam == nullptr || !steam->logged_in)
    {
        return identity;
    }

    if (!HasText(steam->name))
    {
        return identity;
    }

    identity.name = steam->name;
    if (HasPortrait(steam->portrait))
    {
        identity.portrait = steam->portrait;
    }
    return identity;
}

} // namespace csr
