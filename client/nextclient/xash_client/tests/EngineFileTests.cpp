#include "engine_files.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace
{
    int loads, frees, expands;
    bool expand_ok;
    unsigned char* Load(const char* path, int usehunk, int* size)
    {
        ++loads;
        EXPECT_EQ(usehunk, 5);
        if (std::strcmp(path, "missing") == 0) { *size = 0; return nullptr; }
        *size = std::strcmp(path, "negative") == 0 ? -1 : std::strcmp(path, "empty") == 0 ? 0 : 4;
        return new unsigned char[4]{1, 2, 3, 4};
    }
    void Free(void* data) { ++frees; delete[] static_cast<unsigned char*>(data); }
    int Expand(const char* source, char* out, int capacity)
    {
        ++expands;
        EXPECT_STREQ(source, "resource/marlett.ttf");
        constexpr char path[] = "/fixture/valve/resource/marlett.ttf";
        if (!expand_ok || capacity < static_cast<int>(sizeof(path))) return 0;
        std::memcpy(out, path, sizeof(path));
        return 1;
    }
    class EngineFileBridge : public testing::Test
    {
    protected:
        void SetUp() override
        {
            loads = frees = expands = 0; expand_ok = true;
            engine.COM_LoadFile = Load;
            engine.COM_FreeFile = Free;
            engine.COM_ExpandFilename = Expand;
        }
        goldsrc::cl_enginefunc_t engine{};
        csr::client::EngineFiles files;
    };
}

TEST_F(EngineFileBridge, SizeQueriesAndReadsUseTheEngineAllocatorAndReleaseEveryBuffer)
{
    ASSERT_TRUE(files.Bind(&engine));
    EXPECT_EQ(files.Read("gfx/vgui/panel.tga", nullptr, 0), 4);
    unsigned char bytes[4]{};
    EXPECT_EQ(files.Read("gfx/vgui/panel.tga", bytes, sizeof(bytes)), 4);
    EXPECT_EQ(bytes[0], 1); EXPECT_EQ(bytes[3], 4);
    EXPECT_EQ(loads, 2); EXPECT_EQ(frees, 2);
    EXPECT_EQ(files.Read("empty", bytes, sizeof(bytes)), 0);
    EXPECT_EQ(loads, 3); EXPECT_EQ(frees, 3);
}

TEST_F(EngineFileBridge, MissingInvalidAndOversizedFilesCannotPartiallyOverwriteTheDestination)
{
    ASSERT_TRUE(files.Bind(&engine));
    unsigned char bytes[3]{9, 9, 9};
    EXPECT_EQ(files.Read("gfx/vgui/panel.tga", bytes, sizeof(bytes)), -1);
    EXPECT_EQ(files.Read("negative", bytes, sizeof(bytes)), -1);
    EXPECT_EQ(bytes[0], 9); EXPECT_EQ(bytes[2], 9);
    EXPECT_EQ(loads, 2); EXPECT_EQ(frees, 2);
    EXPECT_EQ(files.Read("missing", bytes, sizeof(bytes)), -1);
    EXPECT_EQ(frees, 2);
    EXPECT_EQ(files.Read(nullptr, bytes, sizeof(bytes)), -1);
    EXPECT_EQ(files.Read("fixture", bytes, -1), -1);
    EXPECT_EQ(loads, 3);
}

TEST_F(EngineFileBridge, MarlettUsesTheActualEngineSearchPathAndUnknownFamiliesAreNotGuessed)
{
    ASSERT_TRUE(files.Bind(&engine));
    char path[128]{};
    EXPECT_EQ(files.Font("MARLETT", path, sizeof(path)), 1);
    EXPECT_STREQ(path, "/fixture/valve/resource/marlett.ttf");
    EXPECT_EQ(expands, 1);
    EXPECT_EQ(files.Font("Unknown family", path, sizeof(path)), 0);
    EXPECT_STREQ(path, "");
    EXPECT_EQ(expands, 1);
    expand_ok = false;
    EXPECT_EQ(files.Font("Marlett", path, sizeof(path)), 0);
    EXPECT_EQ(files.Font(nullptr, path, sizeof(path)), 0);
    EXPECT_EQ(files.Font("Marlett", nullptr, sizeof(path)), 0);
    EXPECT_EQ(files.Font("Marlett", path, 0), 0);
}

TEST_F(EngineFileBridge, IncompleteEngineTablesFailAndDiscardAnEarlierBinding)
{
    ASSERT_TRUE(files.Bind(&engine));
    engine.COM_FreeFile = nullptr;
    EXPECT_FALSE(files.Bind(&engine));
    EXPECT_EQ(files.Read("fixture", nullptr, 0), -1);
    EXPECT_EQ(loads, 0);
    char path[32]{};
    EXPECT_EQ(files.Font("Marlett", path, sizeof(path)), 0);
    EXPECT_FALSE(files.Bind(nullptr));
}
