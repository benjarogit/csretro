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
constexpr int kLumpVertexes = 3;
constexpr int kLumpVisibility = 4;
constexpr int kLumpFaces = 7;
constexpr int kLumpLighting = 8;
constexpr int kLumpClipnodes = 9;
constexpr int kLumpEdges = 12;
constexpr int kLumpSurfedges = 13;
constexpr int kLumpModels = 14;

struct DiskVertex
{
    float point[3];
};

struct DiskEdge
{
    unsigned short v[2];
};

struct DiskFace
{
    short planenum;
    short side;
    int firstedge;
    short numedges;
    short texinfo;
    unsigned char styles[4];
    int lightofs;
};

static_assert(sizeof(DiskVertex) == 12);
static_assert(sizeof(goldsrc::mvertex_t) == sizeof(DiskVertex));
static_assert(sizeof(DiskEdge) == 4);
static_assert(sizeof(DiskFace) == 20);
static_assert(sizeof(int) == 4);

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
    std::free(model->vertexes);
    std::free(model->edges);
    std::free(model->surfedges);
    std::free(model->surfaces);
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

bool CopyGeometry(const std::vector<unsigned char>& bytes, const Header& header, goldsrc::model_t* model)
{
    const Lump& vertex_lump = header.lumps[kLumpVertexes];
    if (vertex_lump.length > 0)
    {
        if (static_cast<std::size_t>(vertex_lump.length) % sizeof(DiskVertex) != 0)
        {
            return false;
        }
        const int count = vertex_lump.length / static_cast<int>(sizeof(DiskVertex));
        auto* block = static_cast<goldsrc::mvertex_t*>(std::calloc(static_cast<std::size_t>(count), sizeof(goldsrc::mvertex_t)));
        if (block == nullptr)
        {
            return false;
        }
        std::memcpy(
            block,
            bytes.data() + static_cast<std::size_t>(vertex_lump.offset),
            static_cast<std::size_t>(vertex_lump.length));
        model->vertexes = block;
        model->numvertexes = count;
    }

    const Lump& edge_lump = header.lumps[kLumpEdges];
    if (edge_lump.length > 0)
    {
        if (static_cast<std::size_t>(edge_lump.length) % sizeof(DiskEdge) != 0)
        {
            return false;
        }
        const int count = edge_lump.length / static_cast<int>(sizeof(DiskEdge));
        auto* block = static_cast<goldsrc::medge_t*>(std::calloc(static_cast<std::size_t>(count), sizeof(goldsrc::medge_t)));
        if (block == nullptr)
        {
            return false;
        }
        for (int index = 0; index < count; ++index)
        {
            DiskEdge disk{};
            std::memcpy(
                &disk,
                bytes.data() + static_cast<std::size_t>(edge_lump.offset) + static_cast<std::size_t>(index) * sizeof(DiskEdge),
                sizeof(disk));
            block[index].v[0] = disk.v[0];
            block[index].v[1] = disk.v[1];
        }
        model->edges = block;
        model->numedges = count;
    }

    const Lump& surfedge_lump = header.lumps[kLumpSurfedges];
    if (surfedge_lump.length > 0)
    {
        if (static_cast<std::size_t>(surfedge_lump.length) % sizeof(int) != 0)
        {
            return false;
        }
        const int count = surfedge_lump.length / static_cast<int>(sizeof(int));
        auto* block = static_cast<int*>(std::calloc(static_cast<std::size_t>(count), sizeof(int)));
        if (block == nullptr)
        {
            return false;
        }
        std::memcpy(
            block,
            bytes.data() + static_cast<std::size_t>(surfedge_lump.offset),
            static_cast<std::size_t>(surfedge_lump.length));
        model->surfedges = block;
        model->numsurfedges = count;
    }

    const Lump& face_lump = header.lumps[kLumpFaces];
    if (face_lump.length > 0)
    {
        if (static_cast<std::size_t>(face_lump.length) % sizeof(DiskFace) != 0)
        {
            return false;
        }
        const int count = face_lump.length / static_cast<int>(sizeof(DiskFace));
        auto* block = static_cast<goldsrc::msurface_t*>(std::calloc(static_cast<std::size_t>(count), sizeof(goldsrc::msurface_t)));
        if (block == nullptr)
        {
            return false;
        }
        for (int index = 0; index < count; ++index)
        {
            DiskFace disk{};
            std::memcpy(
                &disk,
                bytes.data() + static_cast<std::size_t>(face_lump.offset) + static_cast<std::size_t>(index) * sizeof(DiskFace),
                sizeof(disk));
            block[index].firstedge = disk.firstedge;
            block[index].numedges = disk.numedges;
        }
        model->surfaces = block;
        model->numsurfaces = count;
    }
    return true;
}

// Vertex, edge, surfedge and face lumps are copied onto the model. A disk
// edge is two vertex indices in 4 bytes; medge_t is wider, so the indices
// are copied field by field. One disk face becomes one surface. Texinfo and
// textures stay unread. Lighting bytes are copied and are not lightmap
// surfaces. GL_BuildLightmaps does not run. eng()->Mod_LoadBrushModel is not
// called.
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
    if (!CopyGeometry(bytes, header, model))
    {
        FreeWorld(model);
        return nullptr;
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
