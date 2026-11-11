#include <gtest/gtest.h>

#include <memory>

#include <nitro_utils/MemoryTools.h>
#include <nitro_utils/platform.h>

#include "SearchConfig.h"

namespace
{
    class ResolverTest : public testing::Test
    {
    protected:
        nitro_utils::SysModule module_{};
        std::unique_ptr<MemoryModule> memory_;
        std::unique_ptr<MemScanner> scanner_;

        void SetUp() override
        {
            module_ = nitro_utils::LoadSysModule(RESOLVER_TEST_MODULE_PATH);
            ASSERT_NE(module_, nullptr);

            memory_ = std::make_unique<MemoryModule>(module_);
            scanner_ = std::make_unique<MemScanner>(*memory_);
        }

        void TearDown() override
        {
            scanner_.reset();
            memory_.reset();

            if (module_ != nullptr)
                nitro_utils::UnloadSysModule(module_);
        }

        uintptr_t Resolve(const nitroapi::SearchConfig& config)
        {
            return config.FindAddress(*scanner_);
        }
    };
}

TEST_F(ResolverTest, ResolvesDynamicSymbol)
{
    const auto expected = reinterpret_cast<uintptr_t>(
        nitro_utils::GetProcAddress(module_, "ResolverTest_Exported"));

    ASSERT_NE(expected, 0u);
    EXPECT_EQ(Resolve(nitroapi::SearchConfig("ResolverTest_Exported", nitroapi::SearchType::SymbolOrPattern)), expected);
}

TEST_F(ResolverTest, ResolvesStaticSymbolFromSymtab)
{
    using GetAddressFn = void* (*)();

    auto get_static_address = reinterpret_cast<GetAddressFn>(
        nitro_utils::GetProcAddress(module_, "ResolverTest_GetStaticAddress"));

    ASSERT_NE(get_static_address, nullptr);

    const auto expected = reinterpret_cast<uintptr_t>(get_static_address());

    ASSERT_NE(expected, 0u);
    EXPECT_EQ(Resolve(nitroapi::SearchConfig("ResolverTest_Static", nitroapi::SearchType::SymbolOrPattern)), expected);
}

TEST_F(ResolverTest, FallsBackToPatternWhenSymbolIsMissing)
{
    using GetAddressFn = void* (*)();

    auto get_pattern_address = reinterpret_cast<GetAddressFn>(
        nitro_utils::GetProcAddress(module_, "ResolverTest_GetPatternTargetAddress"));

    ASSERT_NE(get_pattern_address, nullptr);

    const auto target = reinterpret_cast<uintptr_t>(get_pattern_address());

    const uintptr_t resolved = Resolve(nitroapi::SearchConfig(
        "ResolverTest_NoSuchSymbol",
        nitroapi::SearchType::SymbolOrPattern,
        "23 A1 E7 C5"));

    ASSERT_NE(resolved, 0u);
    EXPECT_GE(resolved, target);
    EXPECT_LT(resolved, target + 64);
}

TEST_F(ResolverTest, MissingSymbolWithoutPatternStaysUnresolved)
{
    EXPECT_EQ(Resolve(nitroapi::SearchConfig("ResolverTest_NoSuchSymbol", nitroapi::SearchType::SymbolOrPattern)), 0u);
}

TEST_F(ResolverTest, MissingSymbolWithUnmatchedPatternStaysUnresolved)
{
    // A pattern that matches nowhere must not turn into a bogus non-zero address
    // through the pattern offset.
    const nitroapi::SearchConfig config(
        "ResolverTest_NoSuchSymbol",
        nitroapi::SearchType::SymbolOrPattern,
        "DE AD BE EF DE AD BE EF DE AD BE EF",
        16);

    EXPECT_EQ(Resolve(config), 0u);
}
