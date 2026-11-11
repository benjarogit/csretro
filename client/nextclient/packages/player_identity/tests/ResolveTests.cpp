#include "player_identity/Resolve.h"

#include <gtest/gtest.h>

namespace
{

constexpr unsigned char kPixel[4] = {1, 2, 3, 4};

csr::SteamPersona LoggedIn(const char* name, const csr::Portrait& portrait)
{
    csr::SteamPersona steam;
    steam.logged_in = true;
    steam.name = name;
    steam.portrait = portrait;
    return steam;
}

} // namespace

TEST(PlayerIdentity, AbsentSteamKeepsTheLocalNameAndHasNoPortrait)
{
    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity("Benny", nullptr);
    EXPECT_STREQ(identity.name, "Benny");
    EXPECT_EQ(identity.portrait.rgba, nullptr);
    EXPECT_EQ(identity.portrait.wide, 0);
    EXPECT_EQ(identity.portrait.tall, 0);
}

TEST(PlayerIdentity, LoggedOutSteamDoesNotLeakNameOrPixels)
{
    const csr::Portrait pixels{kPixel, 2, 2};
    csr::SteamPersona steam = LoggedIn("Steam Benny", pixels);
    steam.logged_in = false;

    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity("Benny", &steam);
    EXPECT_STREQ(identity.name, "Benny");
    EXPECT_EQ(identity.portrait.rgba, nullptr);
}

TEST(PlayerIdentity, LoggedInSteamUsesPersonaNameAndSuppliedPixels)
{
    const csr::Portrait pixels{kPixel, 2, 2};
    const csr::SteamPersona steam = LoggedIn("Steam Benny", pixels);

    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity("Benny", &steam);
    EXPECT_EQ(identity.name, steam.name);
    EXPECT_EQ(identity.portrait.rgba, kPixel);
    EXPECT_EQ(identity.portrait.wide, 2);
    EXPECT_EQ(identity.portrait.tall, 2);
}

TEST(PlayerIdentity, LoggedInSteamWithoutPixelsKeepsTheNameAndDropsThePortrait)
{
    const csr::SteamPersona steam = LoggedIn("Steam Benny", {});

    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity("Benny", &steam);
    EXPECT_STREQ(identity.name, "Steam Benny");
    EXPECT_EQ(identity.portrait.rgba, nullptr);
}

TEST(PlayerIdentity, EmptyPersonaFallsBackToTheLocalName)
{
    const csr::Portrait pixels{kPixel, 2, 2};
    const csr::SteamPersona steam = LoggedIn("", pixels);

    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity("Benny", &steam);
    EXPECT_STREQ(identity.name, "Benny");
    EXPECT_EQ(identity.portrait.rgba, nullptr);
}

TEST(PlayerIdentity, MissingLocalNameIsAnEmptyString)
{
    const csr::PlayerIdentity identity = csr::ResolvePlayerIdentity(nullptr, nullptr);
    EXPECT_STREQ(identity.name, "");
    EXPECT_EQ(identity.portrait.rgba, nullptr);
}
