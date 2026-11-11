#pragma once

#include <string>

namespace nitroapi
{
    enum class BuildFamily
    {
        Unknown,
        PublicLinux,
        PublicWindows,
        // Product process. Xash3D is the engine; retail hw.so is not opened.
        Xash,
    };

    // Identity of the detected game build. It is filled from the build registry
    // in build_registry.h; an empty id means the binaries are not registered.
    struct BuildVersion
    {
        // Steam app build id, 0 when unknown.
        int build_number{};
        // Stable registry id, for example "public-linux-1.0.1.4".
        std::string id;
        // SHA-256 of the engine and client binaries that produced this identity.
        std::string engine_sha256;
        std::string client_sha256;
        BuildFamily family{};

        bool IsSupported() const { return !id.empty(); }
    };
}
