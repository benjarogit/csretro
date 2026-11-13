#include "original_panels.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <dlfcn.h>
#include <memory>
#include <vector>

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;
    class OriginalPanelRuntime : public testing::Test
    {
    protected:
        void SetUp() override
        {
            client.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            vgui.reset(dlopen(CLIENT_VGUI_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(client, nullptr) << dlerror();
            ASSERT_NE(vgui, nullptr) << dlerror();
            auto reset = reinterpret_cast<void (*)()>(dlsym(client.get(), "FixtureClientBindingReset"));
            ASSERT_NE(reset, nullptr);
            reset();
            configure = reinterpret_cast<void (*)(int)>(dlsym(vgui.get(), "FixtureVguiConfigure"));
            ASSERT_NE(configure, nullptr);
            configure(0);
        }
        std::vector<int> Events()
        {
            auto count = reinterpret_cast<int (*)()>(dlsym(vgui.get(), "FixturePanelEventCount"));
            auto event = reinterpret_cast<int (*)(int)>(dlsym(vgui.get(), "FixturePanelEvent"));
            std::vector<int> result;
            if (count && event) for (int i = 0; i < count(); ++i) result.push_back(event(i));
            return result;
        }
        Module client{nullptr, &dlclose}, vgui{nullptr, &dlclose};
        void (*configure)(int){};
        csr::client::OriginalPanels panels;
    };
}

TEST_F(OriginalPanelRuntime, ResolvesNativeRootCoordinatesTreeAndPaintWithoutSdkVtables)
{
    std::string error;
    ASSERT_TRUE(panels.Prepare(client.get(), vgui.get(), error)) << error;
    void* root = panels.Root();
    ASSERT_EQ(reinterpret_cast<uintptr_t>(root), 16u);
    int x{}, y{}, l{}, t{}, r{}, b{};
    panels.AbsPos(root, &x, &y);
    EXPECT_EQ(x, 32); EXPECT_EQ(y, 48);
    panels.Insets(root, &l, &t, &r, &b);
    EXPECT_EQ(l, 1); EXPECT_EQ(t, 2); EXPECT_EQ(r, 3); EXPECT_EQ(b, 4);
    panels.Clip(root, &l, &t, &r, &b);
    EXPECT_EQ(l, 32); EXPECT_EQ(t, 48); EXPECT_EQ(r, 640); EXPECT_EQ(b, 480);
    auto fns = panels.PanelFunctions();
    EXPECT_EQ(fns.child_count(root, fns.user), 2);
    auto popup = fns.child_at(root, 0, fns.user);
    auto hidden = fns.child_at(root, 1, fns.user);
    EXPECT_EQ(fns.is_visible(root, fns.user), 1);
    EXPECT_EQ(fns.is_popup(popup, fns.user), 1);
    EXPECT_EQ(fns.is_visible(hidden, fns.user), 0);
    EXPECT_EQ(fns.child_at(root, 2, fns.user), nullptr);
    fns.solve(root, fns.user);
    fns.apply_scheme(root, fns.user);
    fns.paint(popup, 1, 0, fns.user);
    EXPECT_EQ(Events(), (std::vector<int>{116, 216, 317, 1, 0}));
}

TEST_F(OriginalPanelRuntime, DispatchesOriginalMouseMethodsAndConfiguresPopupInput)
{
    std::string error;
    ASSERT_TRUE(panels.Prepare(client.get(), vgui.get(), error)) << error;
    auto input = panels.InputFunctions();
    input.cursor_moved(137, 89, input.user);
    input.dispatch(input.user);
    input.mouse_pressed(0, input.user);
    input.dispatch(input.user);
    input.mouse_released(0, input.user);
    input.dispatch(input.user);
    panels.Popup(panels.Root(), 1, 0);
    panels.RunFrame();
    EXPECT_EQ(Events(), (std::vector<int>{600, 137, 89, 650, 137, 89, 900, 700, 900, 800, 900, 1016, 1, 2016, 1, 3016, 0, 950}));
}

TEST_F(OriginalPanelRuntime, MissingModulesInterfacesAndSymbolsDoNotCreateAFakeRoot)
{
    std::string error;
    EXPECT_FALSE(panels.Prepare(nullptr, vgui.get(), error));
    EXPECT_EQ(panels.Root(), nullptr);
    configure(3);
    EXPECT_FALSE(panels.Prepare(client.get(), vgui.get(), error));
    EXPECT_EQ(panels.Root(), nullptr);
    configure(0);
    Module empty(dlopen(CLIENT_NO_EXPORT_FIXTURE, RTLD_NOW | RTLD_LOCAL), &dlclose);
    ASSERT_NE(empty, nullptr) << dlerror();
    EXPECT_FALSE(panels.Prepare(empty.get(), vgui.get(), error));
    EXPECT_EQ(panels.Root(), nullptr);
    EXPECT_TRUE(Events().empty());
    EXPECT_TRUE(panels.Prepare(client.get(), vgui.get(), error)) << error;
}
