#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace
{

    std::vector<unsigned char> ReadFile(const char* path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    uint16_t Read16(const std::vector<unsigned char>& file, size_t offset)
    {
        return static_cast<uint16_t>(file[offset] | (file[offset + 1] << 8));
    }

    uint32_t Read32(const std::vector<unsigned char>& file, size_t offset)
    {
        return static_cast<uint32_t>(file[offset]) | (static_cast<uint32_t>(file[offset + 1]) << 8) |
               (static_cast<uint32_t>(file[offset + 2]) << 16) | (static_cast<uint32_t>(file[offset + 3]) << 24);
    }

    uint64_t Read64(const std::vector<unsigned char>& file, size_t offset)
    {
        return static_cast<uint64_t>(Read32(file, offset)) | (static_cast<uint64_t>(Read32(file, offset + 4)) << 32);
    }

    bool IsElf64(const std::vector<unsigned char>& file)
    {
        return file.size() >= 64 && file[0] == 0x7f && file[1] == 'E' && file[2] == 'L' && file[3] == 'F' && file[4] == 2 &&
               file[5] == 1 && Read16(file, 18) == 62;
    }

    bool VirtualToOffset(const std::vector<unsigned char>& file, uint64_t address, uint64_t* offset)
    {
        const uint64_t phoff = Read64(file, 32);
        const uint16_t phentsize = Read16(file, 54);
        const uint16_t phnum = Read16(file, 56);
        for (uint16_t index = 0; index < phnum; ++index)
        {
            const size_t entry = static_cast<size_t>(phoff + static_cast<uint64_t>(index) * phentsize);
            if (entry + 56 > file.size() || Read32(file, entry) != 1)
            {
                continue;
            }
            const uint64_t start = Read64(file, entry + 16);
            const uint64_t file_offset = Read64(file, entry + 8);
            const uint64_t size = Read64(file, entry + 32);
            if (address >= start && address < start + size)
            {
                *offset = file_offset + (address - start);
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> Needed(const std::vector<unsigned char>& file)
    {
        std::vector<std::string> names;
        const uint64_t phoff = Read64(file, 32);
        const uint16_t phentsize = Read16(file, 54);
        const uint16_t phnum = Read16(file, 56);
        uint64_t dyn_offset = 0;
        uint64_t dyn_size = 0;
        for (uint16_t index = 0; index < phnum; ++index)
        {
            const size_t entry = static_cast<size_t>(phoff + static_cast<uint64_t>(index) * phentsize);
            if (entry + 32 > file.size() || Read32(file, entry) != 2)
            {
                continue;
            }
            dyn_offset = Read64(file, entry + 8);
            dyn_size = Read64(file, entry + 32);
        }
        if (dyn_size < 16)
        {
            return names;
        }
        uint64_t strtab = 0;
        for (uint64_t cursor = 0; cursor + 16 <= dyn_size; cursor += 16)
        {
            if (Read64(file, static_cast<size_t>(dyn_offset + cursor)) == 5)
            {
                strtab = Read64(file, static_cast<size_t>(dyn_offset + cursor + 8));
            }
        }
        uint64_t str_offset = 0;
        if (!VirtualToOffset(file, strtab, &str_offset))
        {
            return names;
        }
        for (uint64_t cursor = 0; cursor + 16 <= dyn_size; cursor += 16)
        {
            if (Read64(file, static_cast<size_t>(dyn_offset + cursor)) != 1)
            {
                continue;
            }
            const uint64_t name_offset = str_offset + Read64(file, static_cast<size_t>(dyn_offset + cursor + 8));
            if (name_offset >= file.size())
            {
                continue;
            }
            names.emplace_back(reinterpret_cast<const char*>(file.data() + name_offset));
        }
        return names;
    }

    bool Contains(const std::vector<unsigned char>& file, const char* text)
    {
        const std::string bytes(file.begin(), file.end());
        return bytes.find(text) != std::string::npos;
    }

    bool NeedsHost(const std::vector<std::string>& needed, const char* name)
    {
        for (const std::string& one : needed)
        {
            if (one == name)
            {
                return true;
            }
        }
        return false;
    }

} // namespace

TEST(SdkPair, BothModulesAreElf64)
{
    const std::vector<unsigned char> client = ReadFile(SDK_PAIR_CLIENT);
    const std::vector<unsigned char> vgui = ReadFile(SDK_PAIR_VGUI);
    ASSERT_FALSE(client.empty());
    ASSERT_FALSE(vgui.empty());
    EXPECT_TRUE(IsElf64(client));
    EXPECT_TRUE(IsElf64(vgui));
}

TEST(SdkPair, BothLinkTheHostRuntime)
{
    const std::vector<std::string> client = Needed(ReadFile(SDK_PAIR_CLIENT));
    const std::vector<std::string> vgui = Needed(ReadFile(SDK_PAIR_VGUI));
    for (const char* name : {"libtier0.so", "libvstdlib.so", "libsteam_api.so"})
    {
        EXPECT_TRUE(NeedsHost(client, name)) << name;
        EXPECT_TRUE(NeedsHost(vgui, name)) << name;
    }
}

TEST(SdkPair, BothCarryTheSameSchemeVersion)
{
    const std::vector<unsigned char> client = ReadFile(SDK_PAIR_CLIENT);
    const std::vector<unsigned char> vgui = ReadFile(SDK_PAIR_VGUI);
    EXPECT_TRUE(Contains(client, "VGUI_Scheme009"));
    EXPECT_TRUE(Contains(vgui, "VGUI_Scheme009"));
    EXPECT_TRUE(Contains(vgui, "csr_vgui_dispatch_messages"));
}
