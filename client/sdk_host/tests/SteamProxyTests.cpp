#include "csretro_steam_api.h"
#include "player_identity/Resolve.h"

#include <gtest/gtest.h>
#include <steam/steam_api.h>

TEST(SteamProxy, ReportsLoggedOutWithoutASteamProcess)
{
    EXPECT_FALSE(SteamAPI_IsSteamRunning());
    EXPECT_FALSE(SteamAPI_Init());
    EXPECT_FALSE(SteamAPI_RestartAppIfNecessary(10));
    EXPECT_FALSE(SteamAPI_IsSteamRunning());

    auto* user = static_cast<ISteamUser*>(csr_steam_proxy_interface(STEAMUSER_INTERFACE_VERSION));
    ASSERT_NE(user, nullptr);
    EXPECT_EQ(SteamUser(), user);
    EXPECT_FALSE(user->BLoggedOn());
    EXPECT_FALSE(static_cast<ISteamApps*>(csr_steam_proxy_interface(STEAMAPPS_INTERFACE_VERSION))->BIsVACBanned());

    csr::SteamPersona persona;
    persona.logged_in = true;
    persona.name = "must-not-stick";
    csr_steam_proxy_persona(&persona);
    EXPECT_FALSE(persona.logged_in);
    EXPECT_EQ(persona.name, nullptr);
    EXPECT_EQ(persona.portrait.rgba, nullptr);

    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity("Benny", &persona);
    EXPECT_STREQ(identity.name, "Benny");
    EXPECT_EQ(identity.portrait.rgba, nullptr);
    EXPECT_EQ(identity.portrait.wide, 0);
    EXPECT_EQ(identity.portrait.tall, 0);
}

TEST(SteamProxy, AMissingInterfaceIsNotCalled)
{
    EXPECT_EQ(csr_steam_proxy_interface(nullptr), nullptr);
    void* missing = csr_steam_proxy_interface("NoSuchInterface");
    EXPECT_EQ(missing, nullptr);
    EXPECT_EQ(SteamUtils(), nullptr);
    EXPECT_EQ(SteamMatchmaking(), nullptr);
    EXPECT_EQ(SteamMatchmakingServers(), nullptr);
}
