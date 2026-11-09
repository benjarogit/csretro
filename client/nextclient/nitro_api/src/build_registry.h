#pragma once

#include <optional>
#include <string>

#include "nitroapi/BuildVersion.h"

namespace nitroapi
{
    // Identifies a game build by the content of its binaries, not by file size
    // or a version string. Size and strings change independently from the code
    // and have produced false positives before.
    struct BuildFingerprint
    {
        std::string platform;      // "linux" or "windows"
        std::string engine_sha256; // lowercase hex digest of hw.so / hw.dll
        std::string client_sha256; // lowercase hex digest of client.so / client.dll
    };

    // Returns the registered build for the fingerprint, or std::nullopt when the
    // pair is unknown. An unknown build must never fall back to another one.
    std::optional<BuildVersion> IdentifyBuild(const BuildFingerprint& fingerprint);

    // Returns the lowercase SHA-256 of a file, or an empty string when the file
    // cannot be read.
    std::string HashFileSha256(const std::string& path);
}
