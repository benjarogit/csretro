#include "elf_symbols.h"

#include <goldsrc/client_api.h>
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <memory>
#include <filesystem>
#include <fstream>
#include <elf.h>
#include <link.h>
#include <unistd.h>

namespace
{
    class NativeFunction : public testing::Test
    {
    protected:
        std::unique_ptr<void, int (*)(void*)> module{nullptr, dlclose};
        void SetUp() override
        {
            module.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(module, nullptr) << dlerror();
        }
    };
} // namespace

TEST_F(NativeFunction, ResolvesExportedFunctionInLoadedObject)
{
    EXPECT_EQ(
        nitroapi::FindFunctionAddress(module.get(), "FixtureHudInitCount"),
        reinterpret_cast<uintptr_t>(dlsym(module.get(), "FixtureHudInitCount"))
    );
    EXPECT_NE(nitroapi::FindFunctionAddress(module.get(), "FixtureHudInitCount"), 0u);
}

TEST_F(NativeFunction, ResolvesLocalCallbackRatherThanDependingOnDynsym)
{
    const char* name = "_ZN21CounterStrikeViewport5StartEv";
    EXPECT_EQ(dlsym(module.get(), name), nullptr);
    const auto address = nitroapi::FindFunctionAddress(module.get(), name);
    ASSERT_NE(address, 0u);
    EXPECT_EQ(address, nitroapi::FindSymbolAddress(module.get(), name));
    auto count = reinterpret_cast<int (*)()>(dlsym(module.get(), "FixtureViewportStartCount"));
    ASSERT_NE(count, nullptr);
    const int before = count();
    const auto viewport = nitroapi::FindSymbolAddress(module.get(), "gViewPortInterface");
    ASSERT_NE(viewport, 0u);
    reinterpret_cast<void (*)(void*)>(address)(*reinterpret_cast<void**>(viewport));
    EXPECT_EQ(count(), before + 1);
}

TEST_F(NativeFunction, RejectsDataEvenWhenItIsAnExistingNativeSymbol)
{
    const char* name = "_ZN5vgui216g_pVGuiInterfaceE";
    ASSERT_NE(nitroapi::FindSymbolAddress(module.get(), name), 0u);
    EXPECT_EQ(nitroapi::FindFunctionAddress(module.get(), name), 0u);
}

TEST_F(NativeFunction, RejectsAbsentEmptyAndBorrowedDependencyFunctions)
{
    EXPECT_EQ(nitroapi::FindFunctionAddress(module.get(), "does_not_exist"), 0u);
    EXPECT_EQ(nitroapi::FindFunctionAddress(module.get(), ""), 0u);
    EXPECT_EQ(nitroapi::FindFunctionAddress(module.get(), "malloc"), 0u);
    EXPECT_EQ(nitroapi::FindFunctionAddress(nullptr, "F"), 0u);
}

TEST_F(NativeFunction, KeepsObjectLookupScopedAcrossConsecutiveCalls)
{
    std::unique_ptr<void, int (*)(void*)> other(dlopen(CLIENT_NO_EXPORT_FIXTURE, RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(other, nullptr) << dlerror();
    EXPECT_NE(nitroapi::FindFunctionAddress(other.get(), "FixtureWithoutGameCallbacks"), 0u);
    EXPECT_EQ(nitroapi::FindFunctionAddress(other.get(), "FixtureHudInitCount"), 0u);
    EXPECT_NE(nitroapi::FindFunctionAddress(module.get(), "FixtureHudInitCount"), 0u);
}


namespace
{
    // Copies only this repository's synthetic fixture, never a game library.
    struct ReplacedFixture
    {
        char directory[64] = "/tmp/csretro-native-functions-XXXXXX";
        std::filesystem::path path;
        std::unique_ptr<void, int (*)(void*)> module{nullptr, dlclose};
        ReplacedFixture()
        {
            if (!mkdtemp(directory))
            {
                return;
            }
            path = std::filesystem::path(directory) / "fixture with spaces.so";
            std::filesystem::copy_file(CLIENT_CALLBACK_FIXTURE, path);
            module.reset(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL));
        }
        ~ReplacedFixture()
        {
            module.reset();
            if (!path.empty())
            {
                std::filesystem::remove_all(directory);
            }
        }
        bool ReplaceBuildId(bool remove)
        {
            const auto replacement = path.string() + ".replacement";
            std::filesystem::copy_file(CLIENT_CALLBACK_FIXTURE, replacement);
            std::fstream file(replacement, std::ios::in | std::ios::out | std::ios::binary);
            ElfW(Ehdr) header{};
            file.read(reinterpret_cast<char*>(&header), sizeof(header));
            for (unsigned i = 0; i < header.e_phnum; ++i)
            {
                ElfW(Phdr) program{};
                file.seekg(header.e_phoff + i * sizeof(program));
                file.read(reinterpret_cast<char*>(&program), sizeof(program));
                if (program.p_type != PT_NOTE)
                {
                    continue;
                }
                size_t offset = 0;
                while (offset + sizeof(ElfW(Nhdr)) <= program.p_filesz)
                {
                    ElfW(Nhdr) note{};
                    file.seekg(program.p_offset + offset);
                    file.read(reinterpret_cast<char*>(&note), sizeof(note));
                    const size_t name_size = (note.n_namesz + 3u) & ~3u;
                    if (note.n_type == NT_GNU_BUILD_ID && note.n_descsz)
                    {
                        if (remove)
                        {
                            note.n_type = 0;
                            file.seekp(program.p_offset + offset);
                            file.write(reinterpret_cast<char*>(&note), sizeof(note));
                        }
                        else
                        {
                            const auto location = program.p_offset + offset + sizeof(note) + name_size;
                            char value{};
                            file.seekg(location);
                            file.read(&value, 1);
                            value ^= 0x5a;
                            file.seekp(location);
                            file.write(&value, 1);
                        }
                        file.close();
                        std::filesystem::rename(replacement, path);
                        return true;
                    }
                    offset += sizeof(note) + name_size + ((note.n_descsz + 3u) & ~3u);
                }
            }
            return false;
        }
    };
} // namespace

TEST(NativeFunctionIdentity, RejectsChangedFileBuildIdWithoutReusingOldMetadata)
{
    ReplacedFixture fixture;
    ASSERT_NE(fixture.module, nullptr);
    ASSERT_NE(nitroapi::FindFunctionAddress(fixture.module.get(), "F"), 0u);
    ASSERT_TRUE(fixture.ReplaceBuildId(false));
    EXPECT_EQ(nitroapi::FindFunctionAddress(fixture.module.get(), "F"), 0u);
}

TEST(NativeFunctionIdentity, RejectsFileWithoutBuildId)
{
    ReplacedFixture fixture;
    ASSERT_NE(fixture.module, nullptr);
    ASSERT_TRUE(fixture.ReplaceBuildId(true));
    EXPECT_EQ(nitroapi::FindFunctionAddress(fixture.module.get(), "F"), 0u);
}

TEST(NativeFunctionIdentity, RejectsTruncatedReplacedFile)
{
    ReplacedFixture fixture;
    ASSERT_NE(fixture.module, nullptr);
    const auto replacement = fixture.path.string() + ".replacement";
    std::ofstream(replacement, std::ios::binary) << "invalid ELF";
    std::filesystem::rename(replacement, fixture.path);
    EXPECT_EQ(nitroapi::FindFunctionAddress(fixture.module.get(), "F"), 0u);
}
