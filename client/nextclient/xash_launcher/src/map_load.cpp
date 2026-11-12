#include "xash_launcher/xash_launcher.h"

#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/format/bsp.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
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

constexpr int kLumpEntities = 0;
constexpr int kLumpVisibility = 4;
constexpr int kLumpLighting = 8;
constexpr int kLumpClipnodes = 9;
constexpr int kLumpModels = 14;

static_assert(sizeof(goldsrc::dclipnode_t) == 8);
static_assert(sizeof(goldsrc::dmodel_t) == 64);

goldsrc::model_t* g_owned_world = nullptr;

void FreeWorld(goldsrc::model_t* model)
{
    if (model == nullptr)
    {
        return;
    }
    std::free(model->entities);
    std::free(model->visdata);
    std::free(model->lightdata);
    std::free(model->clipnodes);
    std::free(model->submodels);
    std::free(model);
}

void FreeOwnedAtExit()
{
    FreeWorld(g_owned_world);
    g_owned_world = nullptr;
}

bool CopyLump(const std::vector<unsigned char>& bytes, const Lump& lump, void** dest, bool terminate)
{
    if (lump.length == 0)
    {
        *dest = nullptr;
        return true;
    }
    const std::size_t length = static_cast<std::size_t>(lump.length);
    const std::size_t total = terminate ? length + 1 : length;
    auto* block = static_cast<unsigned char*>(std::malloc(total));
    if (block == nullptr)
    {
        return false;
    }
    std::memcpy(block, bytes.data() + static_cast<std::size_t>(lump.offset), length);
    if (terminate)
    {
        block[length] = '\0';
    }
    *dest = block;
    return true;
}

// Disk faces, texinfo and textures become draw surfaces inside the retail
// brush loader, and that loader needs a renderer. Those lumps stay unread.
// eng()->Mod_LoadBrushModel is not called.
goldsrc::model_t* BuildWorld(const std::vector<unsigned char>& bytes, const char* map_name)
{
    Header header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    auto* model = static_cast<goldsrc::model_t*>(std::calloc(1, sizeof(goldsrc::model_t)));
    if (model == nullptr)
    {
        return nullptr;
    }
    std::snprintf(model->name, sizeof(model->name), "%s", map_name);
    model->type = goldsrc::mod_brush;

    void* entities = nullptr;
    void* visdata = nullptr;
    void* lightdata = nullptr;
    if (!CopyLump(bytes, header.lumps[kLumpEntities], &entities, true) ||
        !CopyLump(bytes, header.lumps[kLumpVisibility], &visdata, false) ||
        !CopyLump(bytes, header.lumps[kLumpLighting], &lightdata, false))
    {
        model->entities = static_cast<char*>(entities);
        model->visdata = static_cast<std::uint8_t*>(visdata);
        model->lightdata = static_cast<goldsrc::color24*>(lightdata);
        FreeWorld(model);
        return nullptr;
    }
    model->entities = static_cast<char*>(entities);
    model->visdata = static_cast<std::uint8_t*>(visdata);
    model->lightdata = static_cast<goldsrc::color24*>(lightdata);

    const Lump& clips = header.lumps[kLumpClipnodes];
    if (clips.length > 0 && static_cast<std::size_t>(clips.length) % sizeof(goldsrc::dclipnode_t) == 0)
    {
        void* clipnodes = nullptr;
        if (!CopyLump(bytes, clips, &clipnodes, false))
        {
            FreeWorld(model);
            return nullptr;
        }
        model->clipnodes = static_cast<goldsrc::dclipnode_t*>(clipnodes);
        model->numclipnodes = clips.length / static_cast<int>(sizeof(goldsrc::dclipnode_t));
    }

    const Lump& models = header.lumps[kLumpModels];
    if (models.length > 0 && static_cast<std::size_t>(models.length) % sizeof(goldsrc::dmodel_t) == 0)
    {
        void* submodels = nullptr;
        if (!CopyLump(bytes, models, &submodels, false))
        {
            FreeWorld(model);
            return nullptr;
        }
        model->submodels = static_cast<goldsrc::dmodel_t*>(submodels);
        model->numsubmodels = models.length / static_cast<int>(sizeof(goldsrc::dmodel_t));
        if (model->numsubmodels > 0)
        {
            model->mins[0] = model->submodels[0].mins[0];
            model->mins[1] = model->submodels[0].mins[1];
            model->mins[2] = model->submodels[0].mins[2];
            model->maxs[0] = model->submodels[0].maxs[0];
            model->maxs[1] = model->submodels[0].maxs[1];
            model->maxs[2] = model->submodels[0].maxs[2];
        }
    }
    return model;
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

    goldsrc::model_t* built = BuildWorld(bytes, map_name);
    if (built == nullptr)
    {
        return 0;
    }
    static bool release_at_exit = false;
    if (!release_at_exit)
    {
        std::atexit(FreeOwnedAtExit);
        release_at_exit = true;
    }

    std::snprintf(state->levelname, sizeof(state->levelname), "%s", map_name);
    state->mapCRC = Checksum(bytes.data(), bytes.size());
    goldsrc::model_t* previous = state->worldmodel;
    state->worldmodel = built;
    if (previous != nullptr && previous == g_owned_world)
    {
        FreeWorld(previous);
    }
    else if (g_owned_world != nullptr && g_owned_world != built)
    {
        FreeWorld(g_owned_world);
    }
    g_owned_world = built;
    return 1;
}

} // namespace csr
