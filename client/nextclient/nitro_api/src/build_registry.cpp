#include "build_registry.h"

#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

#include <data_encoding/sha256.h>

namespace nitroapi
{
    namespace
    {
        struct BuildRecord
        {
            const char* id;
            int build_number;
            const char* platform;
            const char* engine_sha256;
            const char* client_sha256;
        };

        // One entry per supported build. The fingerprints are SHA-256 digests of
        // the untouched retail files. Adding a build means adding its real
        // digests; there is no size- or version-string guessing and no fallback.
        //
        // public-linux-1.0.1.4: Default Public branch, Linux, App 10 build
        // 12934623 (engine "Exe build: 01:35:13 Oct 8 2024").
        constexpr BuildRecord kBuilds[] = {
            {
                "public-linux-1.0.1.4",
                12934623,
                "linux",
                "fca6628b5a4d76a945e11b9796f327004edc65420d9f9cc23f883143508edd78",
                "2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a",
            },
        };

        std::string ToLower(std::string value)
        {
            for (char& c : value)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            return value;
        }

        std::string ToHex(const uint8_t* bytes, size_t size)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0');

            for (size_t i = 0; i < size; ++i)
                stream << std::setw(2) << static_cast<int>(bytes[i]);

            return stream.str();
        }
    }

    std::optional<BuildVersion> IdentifyBuild(const BuildFingerprint& fingerprint)
    {
        const std::string platform = ToLower(fingerprint.platform);
        const std::string engine_sha256 = ToLower(fingerprint.engine_sha256);
        const std::string client_sha256 = ToLower(fingerprint.client_sha256);

        if (platform.empty() || engine_sha256.empty() || client_sha256.empty())
            return std::nullopt;

        for (const BuildRecord& record : kBuilds)
        {
            if (platform != record.platform)
                continue;

            if (engine_sha256 != record.engine_sha256)
                continue;

            if (client_sha256 != record.client_sha256)
                continue;

            BuildVersion build;
            build.build_number = record.build_number;
            build.id = record.id;
            build.engine_sha256 = engine_sha256;
            build.client_sha256 = client_sha256;

            return build;
        }

        return std::nullopt;
    }

    std::string HashFileSha256(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file.is_open())
            return {};

        SHA256_CTX context;
        sha256_init(&context);

        std::array<char, 64 * 1024> buffer{};

        while (file)
        {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize read = file.gcount();

            if (read > 0)
                sha256_update(&context, reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<size_t>(read));
        }

        if (!file.eof())
            return {};

        uint8_t digest[SHA256_BLOCK_SIZE];
        sha256_final(&context, digest);

        return ToHex(digest, sizeof(digest));
    }
}
