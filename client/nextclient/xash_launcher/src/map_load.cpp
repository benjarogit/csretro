#include "xash_launcher/xash_launcher.h"

#include <goldsrc/engine/client_state.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace csr
{
namespace
{

constexpr int kBspVersion = 30;
constexpr int kLumpCount = 15;
constexpr std::size_t kHeaderBytes = 4 + static_cast<std::size_t>(kLumpCount) * 8;
constexpr std::size_t kMaxMapBytes = 32u * 1024u * 1024u;

struct Lump
{
    int offset;
    int length;
};

struct Header
{
    int version;
    Lump lumps[kLumpCount];
};

static_assert(sizeof(Header) == kHeaderBytes);

bool IsDirectory(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    struct stat info
    {
    };
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool MapNameOk(const char* name)
{
    if (name == nullptr || name[0] == '\0')
    {
        return false;
    }
    const std::size_t length = std::strlen(name);
    if (length >= sizeof(goldsrc::client_state_t::levelname))
    {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(name[index]);
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-';
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

std::uint32_t Checksum(const unsigned char* data, std::size_t size)
{
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 0; index < size; ++index)
    {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit)
        {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

bool HeaderFits(const std::vector<unsigned char>& bytes)
{
    if (bytes.size() < kHeaderBytes)
    {
        return false;
    }
    Header header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.version != kBspVersion)
    {
        return false;
    }
    for (const Lump& lump : header.lumps)
    {
        if (lump.offset < 0 || lump.length < 0)
        {
            return false;
        }
        const std::size_t offset = static_cast<std::size_t>(lump.offset);
        const std::size_t length = static_cast<std::size_t>(lump.length);
        if (offset > bytes.size() || length > bytes.size() - offset)
        {
            return false;
        }
    }
    return true;
}

} // namespace

int LoadXashMap(const char* content_root, const char* map_name, goldsrc::client_state_t* state)
{
    if (state == nullptr || !IsDirectory(content_root) || !MapNameOk(map_name))
    {
        return 0;
    }

    const std::string path = std::string(content_root) + "/cstrike/maps/" + map_name + ".bsp";
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        return 0;
    }
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        std::fclose(file);
        return 0;
    }
    const long end = std::ftell(file);
    if (end < static_cast<long>(kHeaderBytes) || static_cast<unsigned long>(end) > kMaxMapBytes)
    {
        std::fclose(file);
        return 0;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0)
    {
        std::fclose(file);
        return 0;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(end));
    const std::size_t got = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (got != bytes.size() || !HeaderFits(bytes))
    {
        return 0;
    }

    std::snprintf(state->levelname, sizeof(state->levelname), "%s", map_name);
    state->mapCRC = Checksum(bytes.data(), bytes.size());
    return 1;
}

} // namespace csr
