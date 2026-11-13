#include "xash_surface/xash_surface.h"

#include <gtest/gtest.h>
#include <vgui/ISurface.h>

#include <string>
#include <vector>

namespace
{

    vgui2::ISurface* AsSurface(csr_surface_t* surface)
    {
        return static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));
    }

    void* Key(unsigned id)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(id));
    }

    unsigned Id(void* panel)
    {
        return static_cast<unsigned>(reinterpret_cast<uintptr_t>(panel));
    }

    struct Walk
    {
        std::vector<std::string> events;
    };

    int Visible(void* panel, void*)
    {
        return Id(panel) == 3 ? 0 : 1;
    }

    int Popup(void* panel, void*)
    {
        const unsigned id = Id(panel);
        return id == 5 || id == 6 ? 1 : 0;
    }

    int ChildCount(void* panel, void*)
    {
        switch (Id(panel))
        {
            case 1:
                return 2;
            case 2:
                return 1;
            case 9:
                return 1;
            default:
                return 0;
        }
    }

    void* ChildAt(void* panel, int index, void*)
    {
        if (Id(panel) == 1 && index == 0)
        {
            return Key(2);
        }
        if (Id(panel) == 1 && index == 1)
        {
            return Key(3);
        }
        if (Id(panel) == 2 && index == 0)
        {
            return Key(4);
        }
        if (Id(panel) == 9)
        {
            return Key(9);
        }
        return nullptr;
    }

    void Solve(void* panel, void* user)
    {
        static_cast<Walk*>(user)->events.push_back("solve " + std::to_string(Id(panel)));
    }

    void Scheme(void* panel, void* user)
    {
        static_cast<Walk*>(user)->events.push_back("scheme " + std::to_string(Id(panel)));
    }

    void Paint(void* panel, int repaint, int allow_force, void* user)
    {
        static_cast<Walk*>(user)->events.push_back(
            "paint " + std::to_string(Id(panel)) + " " + std::to_string(repaint) + " " + std::to_string(allow_force)
        );
    }

    csr_surface_panel_fns_t Fns(Walk* walk)
    {
        csr_surface_panel_fns_t fns{};
        fns.is_visible = Visible;
        fns.is_popup = Popup;
        fns.child_count = ChildCount;
        fns.child_at = ChildAt;
        fns.solve = Solve;
        fns.apply_scheme = Scheme;
        fns.paint = Paint;
        fns.user = walk;
        return fns;
    }

    TEST(SurfaceWalk, WithoutAPanelTableNothingIsVisited)
    {
        csr_surface_t* surface = csr_surface_create(nullptr);
        vgui2::ISurface* vgui = AsSurface(surface);
        vgui->SetEmbeddedPanel(1);
        vgui->SolveTraverse(1, true);
        vgui->PaintTraverse(1);
        EXPECT_TRUE(vgui->ShouldPaintChildPanel(2));
        vgui->MovePopupToFront(5);
        EXPECT_EQ(vgui->GetPopupCount(), 0);
        csr_surface_destroy(surface);
    }

    TEST(SurfaceWalk, SolveVisitsVisibleNodesAndSkipsHiddenBranches)
    {
        Walk walk;
        csr_surface_t* surface = csr_surface_create(nullptr);
        const csr_surface_panel_fns_t fns = Fns(&walk);
        csr_surface_set_panel(surface, &fns);
        vgui2::ISurface* vgui = AsSurface(surface);
        vgui->SetEmbeddedPanel(1);
        vgui->SolveTraverse(1, true);

        ASSERT_EQ(walk.events.size(), 6u);
        EXPECT_EQ(walk.events[0], "scheme 1");
        EXPECT_EQ(walk.events[1], "solve 1");
        EXPECT_EQ(walk.events[2], "scheme 2");
        EXPECT_EQ(walk.events[3], "solve 2");
        EXPECT_EQ(walk.events[4], "scheme 4");
        EXPECT_EQ(walk.events[5], "solve 4");
        csr_surface_destroy(surface);
    }

    TEST(SurfaceWalk, PaintEntersTheRootThenPopups)
    {
        Walk walk;
        csr_surface_t* surface = csr_surface_create(nullptr);
        const csr_surface_panel_fns_t fns = Fns(&walk);
        csr_surface_set_panel(surface, &fns);
        vgui2::ISurface* vgui = AsSurface(surface);
        vgui->SetEmbeddedPanel(1);

        EXPECT_TRUE(vgui->ShouldPaintChildPanel(2));
        EXPECT_FALSE(vgui->ShouldPaintChildPanel(5));
        vgui->MovePopupToFront(2);
        vgui->MovePopupToFront(5);
        EXPECT_EQ(vgui->GetPopupCount(), 1);
        EXPECT_EQ(vgui->GetPopup(0), 5u);

        vgui->PaintTraverse(1);
        ASSERT_EQ(walk.events.size(), 2u);
        EXPECT_EQ(walk.events[0], "paint 1 1 1");
        EXPECT_EQ(walk.events[1], "paint 5 1 1");

        vgui->MovePopupToFront(6);
        EXPECT_EQ(vgui->GetPopup(0), 5u);
        EXPECT_EQ(vgui->GetPopup(1), 6u);
        vgui->MovePopupToBack(6);
        EXPECT_EQ(vgui->GetPopup(0), 6u);
        EXPECT_EQ(vgui->GetPopup(1), 5u);
        vgui->ReleasePanel(5);
        EXPECT_EQ(vgui->GetPopupCount(), 1);
        EXPECT_EQ(vgui->GetPopup(0), 6u);

        walk.events.clear();
        vgui->PaintTraverse(3);
        EXPECT_TRUE(walk.events.empty());
        csr_surface_destroy(surface);
    }

    TEST(SurfaceWalk, ACycleStopsAtTheDepthCap)
    {
        Walk walk;
        csr_surface_t* surface = csr_surface_create(nullptr);
        const csr_surface_panel_fns_t fns = Fns(&walk);
        csr_surface_set_panel(surface, &fns);
        vgui2::ISurface* vgui = AsSurface(surface);
        vgui->SolveTraverse(9, false);
        EXPECT_LE(walk.events.size(), 65u);
        EXPECT_GE(walk.events.size(), 64u);
        csr_surface_destroy(surface);
    }

} // namespace

namespace
{
    TEST(SurfaceWalk, NativeHostPopupAndTraversalApiRegistersInputAndPaintsEachPopupOnce)
    {
        Walk walk;
        csr_surface_t* surface = csr_surface_create(nullptr);
        ASSERT_NE(surface, nullptr);
        auto panel = Fns(&walk);
        csr_surface_set_panel(surface, &panel);
        csr_surface_set_popup(surface, [](void* p, int mouse, int keyboard, void* u) {
            static_cast<Walk*>(u)->events.push_back("popup " + std::to_string(Id(p)) + " " +
                std::to_string(mouse) + " " + std::to_string(keyboard));
        }, &walk);
        auto* vgui = AsSurface(surface);
        csr_surface_set_embedded_panel(surface, Key(1));
        EXPECT_EQ(vgui->GetEmbeddedPanel(), 1u);
        vgui->CreatePopup(5, false, false, false, true, false);
        vgui->CreatePopup(5, false, false, true, true, true);
        vgui->CreatePopup(6, false, false, false, true, true);
        EXPECT_EQ(vgui->GetPopupCount(), 2);
        EXPECT_EQ(walk.events, (std::vector<std::string>{"popup 5 1 0", "popup 5 0 0", "popup 6 1 1"}));
        walk.events.clear();
        csr_surface_solve(surface, Key(1), 0);
        EXPECT_EQ(walk.events, (std::vector<std::string>{"solve 1", "solve 2", "solve 4", "solve 5", "solve 6"}));
        walk.events.clear();
        csr_surface_paint(surface, Key(1));
        EXPECT_EQ(walk.events, (std::vector<std::string>{"paint 1 1 1", "paint 5 1 1", "paint 6 1 1"}));
        vgui->ReleasePanel(5);
        EXPECT_EQ(vgui->GetPopupCount(), 1);
        csr_surface_set_popup(surface, nullptr, nullptr);
        vgui->CreatePopup(0, false, false, false, true, true);
        EXPECT_EQ(vgui->GetPopupCount(), 1);
        csr_surface_set_popup(nullptr, nullptr, nullptr);
        csr_surface_set_embedded_panel(nullptr, nullptr);
        csr_surface_solve(nullptr, nullptr, 0);
        csr_surface_paint(nullptr, nullptr);
        csr_surface_destroy(surface);
    }
}
