#include "xash_host/xash_host.h"

#include "xash_engine/xash_engine.h"

#include <gtest/gtest.h>
#include <Color.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

#include <FileSystem.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace
{

    int g_extra_calls = 0;

    void* ExtraFactory(const char* name, int* code)
    {
        ++g_extra_calls;
        if (code != nullptr)
        {
            *code = 0;
        }
        return const_cast<char*>(name);
    }

    struct InputLog
    {
        std::vector<std::string> events;
    };

    void CursorMoved(void* input, int x, int y)
    {
        static_cast<InputLog*>(input)->events.push_back("move " + std::to_string(x) + " " + std::to_string(y));
    }

    void MousePressed(void* input, int code)
    {
        static_cast<InputLog*>(input)->events.push_back("press " + std::to_string(code));
    }

    void MouseReleased(void* input, int code)
    {
        static_cast<InputLog*>(input)->events.push_back("release " + std::to_string(code));
    }

    void Dispatch(void* vgui)
    {
        static_cast<InputLog*>(vgui)->events.push_back("dispatch");
    }

    class HostFactory : public testing::Test
    {
    protected:
        void TearDown() override
        {
            csr_host_close();
        }
    };

} // namespace

TEST_F(HostFactory, AMissingSurfacePublishesNothing)
{
    EXPECT_EQ(csr_host_open("/no/such/xash_surface.so"), 0);
    csr_host_factories_t factories{};
    EXPECT_EQ(csr_host_factory_list(nullptr, 0, &factories), 0);
    EXPECT_EQ(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);
}

TEST_F(HostFactory, SurfaceFactoryIsFirstAndExtrasAreNotAskedForIt)
{
    g_extra_calls = 0;
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);

    csr_host_factory_fn extra = ExtraFactory;
    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(&extra, 1, &factories), 1);
    ASSERT_EQ(factories.count, 2);
    ASSERT_NE(factories.list, nullptr);

    int code = -1;
    void* surface = factories.list[0]("VGUI_Surface026", &code);
    EXPECT_NE(surface, nullptr);
    EXPECT_EQ(code, 0);
    EXPECT_EQ(factories.list[0]("FileSystem", &code), nullptr);
    EXPECT_EQ(g_extra_calls, 0);

    g_extra_calls = 0;
    EXPECT_EQ(csr_host_create_interface("VGUI_Surface026", nullptr), surface);
    EXPECT_EQ(csr_xash_engine_surface()->object, surface);
    csr_xash_engine_set_text_color(1, 2, 3);
    csr_xash_engine_reset_texture();
    csr_xash_engine_draw_init();
    EXPECT_EQ(g_extra_calls, 0);
    void* filesystem = csr_host_create_interface("FileSystem", nullptr);
    ASSERT_NE(filesystem, nullptr);
    EXPECT_STREQ(static_cast<const char*>(filesystem), "FileSystem");
    EXPECT_EQ(g_extra_calls, 1);
}

TEST_F(HostFactory, MouseEventsDispatchOnceAndCursorWarpsDoNot)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    InputLog log;
    csr_host_vgui_t vgui{};
    vgui.input = &log;
    vgui.vgui = &log;
    vgui.cursor_moved = CursorMoved;
    vgui.mouse_pressed = MousePressed;
    vgui.mouse_released = MouseReleased;
    vgui.dispatch = Dispatch;
    ASSERT_EQ(csr_host_bind_input(&vgui), 1);

    csr_host_input_move(239, 201);
    csr_host_input_button(0, 1);
    csr_host_input_button(0, 0);

    int code = 0;
    auto* surface = static_cast<vgui2::ISurface*>(csr_host_create_interface("VGUI_Surface026", &code));
    ASSERT_NE(surface, nullptr);
    surface->SurfaceSetCursorPos(4, 5);

    ASSERT_EQ(log.events.size(), 6u);
    EXPECT_EQ(log.events[0], "move 239 201");
    EXPECT_EQ(log.events[1], "dispatch");
    EXPECT_EQ(log.events[2], "press 0");
    EXPECT_EQ(log.events[3], "dispatch");
    EXPECT_EQ(log.events[4], "release 0");
    EXPECT_EQ(log.events[5], "dispatch");
    int x = 0;
    int y = 0;
    surface->SurfaceGetCursorPos(x, y);
    EXPECT_EQ(x, 4);
    EXPECT_EQ(y, 5);
}

TEST_F(HostFactory, AMissingPairLibraryLeavesTheSurfaceFactory)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    EXPECT_EQ(csr_host_load_pair("/no/such/client_mini.dll", XASH_VGUI_FILE), 0);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_vgui_dispatch(), nullptr);
    EXPECT_EQ(csr_host_scheme(), nullptr);
    EXPECT_NE(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);

    EXPECT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, "/no/such/vgui2.so"), 0);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_scheme(), nullptr);

    EXPECT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_SURFACE_FILE), 0);
    EXPECT_EQ(csr_host_vgui_dispatch(), nullptr);
}

TEST_F(HostFactory, ThePairPublishesTheSurfaceAndTheOneScheme)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    EXPECT_EQ(csr_host_pair_loaded(), 1);

    void* surface = csr_host_create_interface("VGUI_Surface026", nullptr);
    void* scheme = csr_host_scheme();
    ASSERT_NE(surface, nullptr);
    ASSERT_NE(scheme, nullptr);
    EXPECT_NE(scheme, surface);
    ASSERT_NE(csr_host_vgui_dispatch(), nullptr);

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    EXPECT_EQ(factories.count, 1);
    EXPECT_EQ(factories.list[0]("VGUI_Surface026", nullptr), surface);
}

TEST_F(HostFactory, GameUiRequiresTheSurfaceFactory)
{
    EXPECT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 0);
    EXPECT_EQ(csr_host_gameui_loaded(), 0);

    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    EXPECT_EQ(csr_host_load_gameui("/no/such/gameui.so"), 0);
    EXPECT_EQ(csr_host_gameui(), nullptr);
    EXPECT_EQ(csr_host_game_console(), nullptr);
    EXPECT_NE(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);

    EXPECT_EQ(csr_host_load_gameui(XASH_SURFACE_FILE), 0);
    EXPECT_EQ(csr_host_gameui_loaded(), 0);
}

TEST_F(HostFactory, GameUiUsesTheExistingSurfaceFactory)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    void* surface = csr_host_create_interface("VGUI_Surface026", nullptr);
    ASSERT_NE(surface, nullptr);

    ASSERT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 1);
    EXPECT_EQ(csr_host_gameui_loaded(), 1);
    ASSERT_NE(csr_host_gameui(), nullptr);
    ASSERT_NE(csr_host_game_console(), nullptr);
    EXPECT_NE(csr_host_gameui(), surface);
    EXPECT_NE(csr_host_game_console(), surface);
    EXPECT_EQ(csr_host_create_interface("GameUI007", nullptr), nullptr);
    EXPECT_EQ(csr_host_create_interface("VGUI_Surface026", nullptr), surface);

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    EXPECT_EQ(factories.count, 1);
}

struct TempRoot
{
    std::string root;

    TempRoot()
    {
        char name[] = "/tmp/csretro-fs-XXXXXX";
        if (mkdtemp(name) != nullptr)
        {
            root = name;
        }
    }

    ~TempRoot()
    {
        if (root.empty())
        {
            return;
        }
        std::remove((root + "/maps/demo.txt").c_str());
        std::remove((root + "/leak").c_str());
        rmdir((root + "/maps").c_str());
        rmdir(root.c_str());
    }
};

TEST_F(HostFactory, FilesystemRequiresTheSurfaceFactory)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    EXPECT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 0);
    EXPECT_EQ(csr_host_filesystem_mounted(), 0);
    EXPECT_EQ(csr_host_create_interface("VFileSystem009", nullptr), nullptr);

    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-fs-missing-%d", static_cast<int>(getpid()));
    EXPECT_EQ(csr_host_mount_filesystem(missing), 0);
    EXPECT_EQ(csr_host_filesystem_mounted(), 0);
    EXPECT_EQ(csr_host_create_interface("VFileSystem009", nullptr), nullptr);
    EXPECT_NE(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    EXPECT_EQ(factories.count, 1);
}

TEST_F(HostFactory, FilesystemServesOnlyTheMountedRoot)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    void* surface = csr_host_create_interface("VGUI_Surface026", nullptr);
    ASSERT_NE(surface, nullptr);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/maps").c_str(), 0755), 0);
    FILE* out = std::fopen((temp.root + "/maps/demo.txt").c_str(), "wb");
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(std::fwrite("hello", 1, 5, out), 5u);
    std::fclose(out);
    ASSERT_EQ(symlink("/etc/passwd", (temp.root + "/leak").c_str()), 0);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    EXPECT_EQ(csr_host_filesystem_mounted(), 1);
    EXPECT_EQ(csr_host_vgui_connected(), 0);

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    ASSERT_EQ(factories.count, 2);
    EXPECT_EQ(factories.list[0]("VGUI_Surface026", nullptr), surface);
    EXPECT_EQ(factories.list[0]("VFileSystem009", nullptr), nullptr);

    int code = -1;
    auto* filesystem = static_cast<IFileSystem*>(factories.list[1]("VFileSystem009", &code));
    ASSERT_NE(filesystem, nullptr);
    EXPECT_EQ(code, 0);
    EXPECT_EQ(factories.list[1]("VGUI_Surface026", nullptr), nullptr);
    EXPECT_EQ(csr_host_create_interface("VFileSystem009", nullptr), filesystem);
    EXPECT_EQ(csr_host_create_interface("VGUI_Surface026", nullptr), surface);

    EXPECT_TRUE(filesystem->FileExists("maps/demo.txt"));
    EXPECT_TRUE(filesystem->IsDirectory("maps"));
    EXPECT_EQ(filesystem->Size("maps/demo.txt"), 5u);
    FileHandle_t handle = filesystem->Open("maps/demo.txt", "rb", nullptr);
    ASSERT_NE(handle, nullptr);
    char bytes[8] = {};
    EXPECT_EQ(filesystem->Read(bytes, 5, handle), 5);
    EXPECT_STREQ(bytes, "hello");
    filesystem->Close(handle);

    EXPECT_FALSE(filesystem->FileExists("maps/missing.txt"));
    EXPECT_EQ(filesystem->Open("maps/missing.txt", "rb", nullptr), nullptr);
    EXPECT_EQ(filesystem->Size("maps/missing.txt"), static_cast<unsigned int>(-1));
    EXPECT_FALSE(filesystem->FileExists("../etc/passwd"));
    EXPECT_EQ(filesystem->Open("/etc/passwd", "rb", nullptr), nullptr);
    EXPECT_EQ(filesystem->Open("maps/../../etc/passwd", "rb", nullptr), nullptr);
    EXPECT_FALSE(filesystem->FileExists("leak"));
    EXPECT_EQ(filesystem->Open("leak", "rb", nullptr), nullptr);
}

TEST_F(HostFactory, TextureFileReadsUncompressedTgaFromTheRoot)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    auto* vgui = static_cast<vgui2::ISurface*>(csr_host_create_interface("VGUI_Surface026", nullptr));
    ASSERT_NE(vgui, nullptr);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    unsigned char tga[24] = {};
    tga[2] = 2;
    tga[12] = 2;
    tga[14] = 1;
    tga[16] = 24;
    tga[18] = 0;
    tga[19] = 0;
    tga[20] = 255;
    tga[21] = 255;
    tga[22] = 0;
    tga[23] = 0;
    const std::string path = temp.root + "/cursor.tga";
    FILE* out = std::fopen(path.c_str(), "wb");
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(std::fwrite(tga, 1, sizeof(tga), out), sizeof(tga));
    std::fclose(out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    vgui->DrawSetTextureFile(3, "cursor.tga", 0, false);
    int wide = 0;
    int tall = 0;
    vgui->DrawGetTextureSize(3, wide, tall);
    EXPECT_EQ(wide, 2);
    EXPECT_EQ(tall, 1);

    vgui->DrawSetTextureFile(4, "missing.tga", 0, false);
    wide = 7;
    tall = 7;
    vgui->DrawGetTextureSize(4, wide, tall);
    EXPECT_EQ(wide, 0);
    EXPECT_EQ(tall, 0);

    vgui->DrawSetTextureFile(5, "../cursor.tga", 0, false);
    wide = 7;
    tall = 7;
    vgui->DrawGetTextureSize(5, wide, tall);
    EXPECT_EQ(wide, 0);
    EXPECT_EQ(tall, 0);

    vgui->DrawSetTextureFile(6, "cursor", 0, false);
    wide = 0;
    tall = 0;
    vgui->DrawGetTextureSize(6, wide, tall);
    EXPECT_EQ(wide, 2);
    EXPECT_EQ(tall, 1);

    vgui->DrawSetTextureFile(7, "../cursor", 0, false);
    wide = 7;
    tall = 7;
    vgui->DrawGetTextureSize(7, wide, tall);
    EXPECT_EQ(wide, 0);
    EXPECT_EQ(tall, 0);

    std::remove(path.c_str());
}

TEST_F(HostFactory, ConnectsTheSurfaceIntoVguiWithoutRunFrame)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    EXPECT_EQ(csr_host_connect_vgui(), 0);

    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    EXPECT_EQ(csr_host_connect_vgui(), 0);
    EXPECT_EQ(csr_host_vgui_connected(), 0);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    EXPECT_EQ(csr_host_vgui_connected(), 1);
    EXPECT_EQ(csr_host_connect_vgui(), 1);
    EXPECT_NE(csr_host_vgui_dispatch(), nullptr);
    EXPECT_NE(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);
    EXPECT_EQ(csr_host_load_scheme("resource/scheme.res", "ClientScheme"), 0);
    EXPECT_EQ(csr_host_scheme_loaded(), 0);
}

TEST_F(HostFactory, LoadsASchemeFromTheMountedRoot)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/resource").c_str(), 0755), 0);
    const std::string path = temp.root + "/resource/scheme.res";
    FILE* out = std::fopen(path.c_str(), "wb");
    ASSERT_NE(out, nullptr);
    const char* text =
        "\"Scheme\"\n"
        "{\n"
        "\t\"Colors\"\n"
        "\t{\n"
        "\t\t\"Armed\" \"10 20 30 40\"\n"
        "\t}\n"
        "\t\"BaseSettings\"\n"
        "\t{\n"
        "\t\t\"Button.ArmedTextColor\" \"Armed\"\n"
        "\t}\n"
        "}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), out), std::strlen(text));
    std::fclose(out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    EXPECT_EQ(csr_host_load_scheme("resource/scheme.res", "ClientScheme"), 0);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    ASSERT_EQ(csr_host_load_scheme("resource/scheme.res", "ClientScheme"), 1);
    EXPECT_EQ(csr_host_scheme_loaded(), 1);

    auto* manager = static_cast<vgui2::ISchemeManager*>(csr_host_scheme());
    ASSERT_NE(manager, nullptr);
    vgui2::IScheme* scheme = manager->GetIScheme(manager->GetScheme("ClientScheme"));
    ASSERT_NE(scheme, nullptr);
    const Color color = scheme->GetColor("Armed", Color(0, 0, 0, 0));
    EXPECT_EQ(color.r(), 10);
    EXPECT_EQ(color.g(), 20);
    EXPECT_EQ(color.b(), 30);
    EXPECT_EQ(color.a(), 40);

    std::remove(path.c_str());
    rmdir((temp.root + "/resource").c_str());
}

TEST_F(HostFactory, InitializesGameUiWithoutRunFrame)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    ASSERT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 1);
    EXPECT_EQ(csr_host_init_gameui(), 0);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* out = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(out, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), out), std::strlen(text));
    std::fclose(out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    ASSERT_EQ(csr_host_init_gameui(), 1);
    EXPECT_EQ(csr_host_gameui_initialized(), 1);
    EXPECT_GE(csr_host_gameui_child_count(), 1);
    EXPECT_EQ(csr_host_gameui_base_child_count(), 2);
    EXPECT_EQ(csr_host_init_gameui(), 1);

    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}

TEST_F(HostFactory, PaintsTheGameUiTreeWithoutRunFrame)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    ASSERT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 1);
    EXPECT_EQ(csr_host_paint_gameui(640, 480), 0);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* out = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(out, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), out), std::strlen(text));
    std::fclose(out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    ASSERT_EQ(csr_host_init_gameui(), 1);
    ASSERT_EQ(csr_host_paint_gameui(640, 480), 1);
    EXPECT_EQ(csr_host_textured_quad_count(), 0);
    EXPECT_EQ(csr_host_paint_gameui(0, 480), 0);

    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}

TEST_F(HostFactory, BuildsTheServerListWhenTheDefaultSchemeIsPresent)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    ASSERT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 1);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource/schemes").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    const std::string scheme = temp.root + "/resource/schemes/TrackerScheme_ClassicPlus.res";
    const char* menu_text = "\"GameMenu\"\n{\n}\n";
    const char* scheme_text =
        "\"Scheme\"\n"
        "{\n"
        "\t\"Colors\"\n"
        "\t{\n"
        "\t\t\"Blank\" \"0 0 0 0\"\n"
        "\t}\n"
        "\t\"BaseSettings\"\n"
        "\t{\n"
        "\t\t\"FgColor\" \"Blank\"\n"
        "\t}\n"
        "}\n";
    FILE* menu_out = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_out, nullptr);
    ASSERT_EQ(std::fwrite(menu_text, 1, std::strlen(menu_text), menu_out), std::strlen(menu_text));
    std::fclose(menu_out);
    FILE* scheme_out = std::fopen(scheme.c_str(), "wb");
    ASSERT_NE(scheme_out, nullptr);
    ASSERT_EQ(std::fwrite(scheme_text, 1, std::strlen(scheme_text), scheme_out), std::strlen(scheme_text));
    std::fclose(scheme_out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    ASSERT_EQ(csr_host_init_gameui(), 1);
    EXPECT_EQ(csr_host_gameui_initialized(), 1);
    EXPECT_EQ(csr_host_gameui_base_child_count(), 3);

    std::remove(menu.c_str());
    std::remove(scheme.c_str());
    rmdir((temp.root + "/resource/schemes").c_str());
    rmdir((temp.root + "/resource").c_str());
    rmdir((temp.root + "/Resource").c_str());
}

TEST_F(HostFactory, ParsesTheBackgroundLayoutAndLoadsItsTga)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    ASSERT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 1);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource/schemes").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/gfx").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/gfx/vgui").c_str(), 0755), 0);

    const std::string menu = temp.root + "/Resource/GameMenu.res";
    const std::string scheme = temp.root + "/resource/schemes/TrackerScheme_ClassicPlus.res";
    const std::string layout = temp.root + "/resource/BackgroundLayout.txt";
    const std::string image = temp.root + "/gfx/vgui/round_corner_nw.tga";
    FILE* menu_out = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_out, nullptr);
    const char* menu_text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(menu_text, 1, std::strlen(menu_text), menu_out), std::strlen(menu_text));
    std::fclose(menu_out);

    FILE* scheme_out = std::fopen(scheme.c_str(), "wb");
    ASSERT_NE(scheme_out, nullptr);
    const char* scheme_text =
        "\"Scheme\"\n"
        "{\n"
        "\t\"Colors\"\n"
        "\t{\n"
        "\t\t\"Blank\" \"0 0 0 0\"\n"
        "\t}\n"
        "\t\"BaseSettings\"\n"
        "\t{\n"
        "\t\t\"FgColor\" \"Blank\"\n"
        "\t}\n"
        "}\n";
    ASSERT_EQ(std::fwrite(scheme_text, 1, std::strlen(scheme_text), scheme_out), std::strlen(scheme_text));
    std::fclose(scheme_out);

    FILE* layout_out = std::fopen(layout.c_str(), "wb");
    ASSERT_NE(layout_out, nullptr);
    const char* layout_text =
        "resolution\t640\t480\n"
        "gfx/vgui/round_corner_nw.tga\tscaled\t0\t0\n"
        "gfx/vgui/missing.tga\tscaled\t1\t1\n";
    ASSERT_EQ(std::fwrite(layout_text, 1, std::strlen(layout_text), layout_out), std::strlen(layout_text));
    std::fclose(layout_out);

    unsigned char tga[24] = {};
    tga[2] = 2;
    tga[12] = 2;
    tga[14] = 1;
    tga[16] = 24;
    tga[20] = 255;
    FILE* image_out = std::fopen(image.c_str(), "wb");
    ASSERT_NE(image_out, nullptr);
    ASSERT_EQ(std::fwrite(tga, 1, sizeof(tga), image_out), sizeof(tga));
    std::fclose(image_out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    auto* files = static_cast<IFileSystem*>(csr_host_create_interface("VFileSystem009", nullptr));
    ASSERT_NE(files, nullptr);
    char bytes[] = "resolution 640 // note\n\"gfx/vgui/round_corner_nw.tga\"";
    char token[512];
    bool quoted = false;
    char* next = files->ParseFile(bytes, token, &quoted);
    ASSERT_NE(next, nullptr);
    EXPECT_STREQ(token, "resolution");
    EXPECT_FALSE(quoted);
    next = files->ParseFile(next, token, &quoted);
    EXPECT_STREQ(token, "640");
    next = files->ParseFile(next, token, &quoted);
    EXPECT_STREQ(token, "gfx/vgui/round_corner_nw.tga");
    EXPECT_TRUE(quoted);
    EXPECT_EQ(files->ParseFile(next, token, &quoted), nullptr);

    EXPECT_EQ(csr_host_start_gameui(), 0);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    ASSERT_EQ(csr_host_init_gameui(), 1);
    const int quads_before = csr_host_textured_quad_count();
    ASSERT_EQ(csr_host_paint_gameui(640, 480), 1);
    EXPECT_EQ(csr_host_textured_quad_count(), quads_before + 1);
    ASSERT_EQ(csr_host_start_gameui(), 1);
    EXPECT_EQ(csr_host_gameui_started(), 1);
    EXPECT_EQ(csr_host_engine_command_count(), 5);
    ASSERT_EQ(csr_host_paint_gameui(640, 480), 1);
    EXPECT_GT(csr_host_textured_quad_count(), quads_before + 1);
    EXPECT_EQ(csr_host_start_gameui(), 1);

    auto* vgui = static_cast<vgui2::ISurface*>(csr_host_create_interface("VGUI_Surface026", nullptr));
    ASSERT_NE(vgui, nullptr);
    int matches = 0;
    for (int id = 1; id < 64; ++id)
    {
        int wide = 0;
        int tall = 0;
        vgui->DrawGetTextureSize(id, wide, tall);
        if (wide == 2 && tall == 1)
        {
            ++matches;
        }
    }
    EXPECT_EQ(matches, 1);

    std::remove(menu.c_str());
    std::remove(scheme.c_str());
    std::remove(layout.c_str());
    std::remove(image.c_str());
    rmdir((temp.root + "/gfx/vgui").c_str());
    rmdir((temp.root + "/gfx").c_str());
    rmdir((temp.root + "/resource/schemes").c_str());
    rmdir((temp.root + "/resource").c_str());
    rmdir((temp.root + "/Resource").c_str());
}

TEST_F(HostFactory, StartsGameUiWhenTheSchemeFileIsMissing)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    ASSERT_EQ(csr_host_load_gameui(XASH_GAMEUI_FILE), 1);
    EXPECT_EQ(csr_host_start_gameui(), 0);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* out = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(out, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), out), std::strlen(text));
    std::fclose(out);

    ASSERT_EQ(csr_host_mount_filesystem(temp.root.c_str()), 1);
    ASSERT_EQ(csr_host_connect_vgui(), 1);
    ASSERT_EQ(csr_host_init_gameui(), 1);
    ASSERT_EQ(csr_host_start_gameui(), 1);
    EXPECT_EQ(csr_host_gameui_started(), 1);
    EXPECT_EQ(csr_host_engine_command_count(), 5);
    EXPECT_EQ(csr_host_start_gameui(), 1);
    EXPECT_EQ(csr_host_engine_command_count(), 5);

    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}
