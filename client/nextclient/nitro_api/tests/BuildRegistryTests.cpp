#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "build_registry.h"

namespace
{
    std::filesystem::path WriteTempFile(const std::string& name, const std::string& content)
    {
        const std::filesystem::path path = std::filesystem::temp_directory_path() / ("csretro-build-registry-" + name + ".bin");

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));

        return path;
    }
}

TEST(BuildRegistry, HashesFileContent)
{
    // SHA-256 of "abc" (FIPS 180-2 test vector).
    const std::filesystem::path path = WriteTempFile("abc", "abc");

    EXPECT_EQ(nitroapi::HashFileSha256(path.string()),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::filesystem::remove(path);
}

TEST(BuildRegistry, ReturnsEmptyForMissingFile)
{
    EXPECT_TRUE(nitroapi::HashFileSha256("/nonexistent/csretro/hw.so").empty());
}

TEST(BuildRegistry, RejectsUnknownFingerprint)
{
    nitroapi::BuildFingerprint fingerprint{
        "linux",
        std::string(64, 'a'),
        std::string(64, 'b'),
    };

    EXPECT_FALSE(nitroapi::IdentifyBuild(fingerprint).has_value());
}

TEST(BuildRegistry, RejectsIncompleteFingerprint)
{
    EXPECT_FALSE(nitroapi::IdentifyBuild({"linux", "", ""}).has_value());
    EXPECT_FALSE(nitroapi::IdentifyBuild({"", std::string(64, 'a'), std::string(64, 'b')}).has_value());
}

TEST(BuildRegistry, RejectsMismatchedPlatform)
{
    nitroapi::BuildFingerprint fingerprint{
        "windows",
        "fca6628b5a4d76a945e11b9796f327004edc65420d9f9cc23f883143508edd78",
        "2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a",
    };

    EXPECT_FALSE(nitroapi::IdentifyBuild(fingerprint).has_value());
}

TEST(BuildRegistry, IdentifiesDefaultPublicLinux)
{
    // Must match the Default Public entry in build_registry.cpp.
    nitroapi::BuildFingerprint fingerprint{
        "linux",
        "fca6628b5a4d76a945e11b9796f327004edc65420d9f9cc23f883143508edd78",
        "2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a",
    };

    const std::optional<nitroapi::BuildVersion> build = nitroapi::IdentifyBuild(fingerprint);

    ASSERT_TRUE(build.has_value());
    EXPECT_EQ(build->id, "public-linux-1.0.1.4");
    EXPECT_EQ(build->build_number, 12934623);
    EXPECT_TRUE(build->IsSupported());
    EXPECT_EQ(build->engine_sha256, fingerprint.engine_sha256);
    EXPECT_EQ(build->client_sha256, fingerprint.client_sha256);
}

TEST(BuildRegistry, FingerprintComparisonIsCaseInsensitive)
{
    nitroapi::BuildFingerprint fingerprint{
        "LINUX",
        "FCA6628B5A4D76A945E11B9796F327004EDC65420D9F9CC23F883143508EDD78",
        "2E799EF31931C0F4372FF31AD30F0AAB529B16461ED3B0928DFBD958183B2E5A",
    };

    const std::optional<nitroapi::BuildVersion> build = nitroapi::IdentifyBuild(fingerprint);

    ASSERT_TRUE(build.has_value());
    EXPECT_EQ(build->engine_sha256, "fca6628b5a4d76a945e11b9796f327004edc65420d9f9cc23f883143508edd78");
}
