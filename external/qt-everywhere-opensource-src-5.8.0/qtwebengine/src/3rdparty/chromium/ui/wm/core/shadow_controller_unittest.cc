// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_state.h"
#include "ui/wm/public/activation_client.h"

namespace wm {

class ShadowControllerTest : public aura::test::AuraTestBase {
 public:
  ShadowControllerTest() {}
  ~ShadowControllerTest() override {}

  void SetUp() override {
    wm_state_.reset(new wm::WMState);
    AuraTestBase::SetUp();
    new wm::DefaultActivationClient(root_window());
    aura::client::ActivationClient* activation_client =
        aura::client::GetActivationClient(root_window());
    shadow_controller_.reset(new ShadowController(activation_client));
  }
  void TearDown() override {
    shadow_controller_.reset();
    AuraTestBase::TearDown();
    wm_state_.reset();
  }

 protected:
  ShadowController* shadow_controller() { return shadow_controller_.get(); }

  void ActivateWindow(aura::Window* window) {
    DCHECK(window);
    DCHECK(window->GetRootWindow());
    aura::client::GetActivationClient(window->GetRootWindow())->ActivateWindow(
        window);
  }

 private:
  std::unique_ptr<ShadowController> shadow_controller_;
  std::unique_ptr<wm::WMState> wm_state_;

  DISALLOW_COPY_AND_ASSIGN(ShadowControllerTest);
};

// Tests that various methods in Window update the Shadow object as expected.
TEST_F(ShadowControllerTest, Shadow) {
  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());

  // We should create the shadow before the window is visible (the shadow's
  // layer won't get drawn yet since it's a child of the window's layer).
  const Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow should remain visible after window visibility changes.
  window->Show();
  EXPECT_TRUE(shadow->layer()->visible());
  window->Hide();
  EXPECT_TRUE(shadow->layer()->visible());

  // If the shadow is disabled, it should be hidden.
  SetShadowType(window.get(), SHADOW_TYPE_NONE);
  window->Show();
  EXPECT_FALSE(shadow->layer()->visible());
  SetShadowType(window.get(), SHADOW_TYPE_RECTANGULAR);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow's layer should be a child of the window's layer.
  EXPECT_EQ(window->layer(), shadow->layer()->parent());
}

// Tests that the window's shadow's bounds are updated correctly.
TEST_F(ShadowControllerTest, ShadowBounds) {
  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->Show();

  const gfx::Rect kOldBounds(20, 30, 400, 300);
  window->SetBounds(kOldBounds);

  // When the shadow is first created, it should use the window's size (but
  // remain at the origin, since it's a child of the window's layer).
  SetShadowType(window.get(), SHADOW_TYPE_RECTANGULAR);
  const Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(gfx::Rect(kOldBounds.size()).ToString(),
            shadow->content_bounds().ToString());

  // When we change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window->SetBounds(kNewBounds);
  EXPECT_EQ(gfx::Rect(kNewBounds.size()).ToString(),
            shadow->content_bounds().ToString());
}

// Tests that activating a window changes the shadow style.
TEST_F(ShadowControllerTest, ShadowStyle) {
  std::unique_ptr<aura::Window> window1(new aura::Window(NULL));
  window1->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  ParentWindow(window1.get());
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  Shadow* shadow1 = ShadowController::GetShadowForWindow(window1.get());
  ASSERT_TRUE(shadow1 != NULL);
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow1->style());

  // Create another window and activate it.
  std::unique_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  ParentWindow(window2.get());
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  window2->Show();
  ActivateWindow(window2.get());

  // window1 is now inactive, so shadow should go inactive.
  Shadow* shadow2 = ShadowController::GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow2 != NULL);
  EXPECT_EQ(Shadow::STYLE_INACTIVE, shadow1->style());
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow2->style());
}

// Tests that shadow gets updated when the window show state changes.
TEST_F(ShadowControllerTest, ShowState) {
  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->Show();

  Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(Shadow::STYLE_INACTIVE, shadow->style());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_FALSE(shadow->layer()->visible());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(shadow->layer()->visible());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(shadow->layer()->visible());
}

// Tests that we use smaller shadows for tooltips and menus.
TEST_F(ShadowControllerTest, SmallShadowsForTooltipsAndMenus) {
  std::unique_ptr<aura::Window> tooltip_window(new aura::Window(NULL));
  tooltip_window->SetType(ui::wm::WINDOW_TYPE_TOOLTIP);
  tooltip_window->Init(ui::LAYER_TEXTURED);
  ParentWindow(tooltip_window.get());
  tooltip_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  tooltip_window->Show();

  Shadow* tooltip_shadow =
      ShadowController::GetShadowForWindow(tooltip_window.get());
  ASSERT_TRUE(tooltip_shadow != NULL);
  EXPECT_EQ(Shadow::STYLE_SMALL, tooltip_shadow->style());

  std::unique_ptr<aura::Window> menu_window(new aura::Window(NULL));
  menu_window->SetType(ui::wm::WINDOW_TYPE_MENU);
  menu_window->Init(ui::LAYER_TEXTURED);
  ParentWindow(menu_window.get());
  menu_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  menu_window->Show();

  Shadow* menu_shadow =
      ShadowController::GetShadowForWindow(tooltip_window.get());
  ASSERT_TRUE(menu_shadow != NULL);
  EXPECT_EQ(Shadow::STYLE_SMALL, menu_shadow->style());
}

// http://crbug.com/120210 - transient parents of certain types of transients
// should not lose their shadow when they lose activation to the transient.
TEST_F(ShadowControllerTest, TransientParentKeepsActiveShadow) {
  std::unique_ptr<aura::Window> window1(new aura::Window(NULL));
  window1->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  ParentWindow(window1.get());
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  Shadow* shadow1 = ShadowController::GetShadowForWindow(window1.get());
  ASSERT_TRUE(shadow1 != NULL);
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow1->style());

  // Create a window that is transient to window1, and that has the 'hide on
  // deactivate' property set. Upon activation, window1 should still have an
  // active shadow.
  std::unique_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  ParentWindow(window2.get());
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  AddTransientChild(window1.get(), window2.get());
  aura::client::SetHideOnDeactivate(window2.get(), true);
  window2->Show();
  ActivateWindow(window2.get());

  // window1 is now inactive, but its shadow should still appear active.
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow1->style());
}

}  // namespace wm
