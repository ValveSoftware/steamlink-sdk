// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/visibility_controller.h"

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace wm {

typedef aura::test::AuraTestBase VisibilityControllerTest;

// Check that a transparency change to 0 will not cause a hide call to be
// ignored.
TEST_F(VisibilityControllerTest, AnimateTransparencyToZeroAndHideHides) {
  // We cannot disable animations for this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  VisibilityController controller;
  aura::client::SetVisibilityClient(root_window(), &controller);

  SetChildWindowVisibilityChangesAnimated(root_window());

  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &d, -2, gfx::Rect(0, 0, 50, 50), root_window()));
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(5));

  EXPECT_TRUE(window->layer()->visible());
  EXPECT_TRUE(window->IsVisible());

  window->layer()->SetOpacity(0.0);
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->GetAnimator()->
      IsAnimatingProperty(ui::LayerAnimationElement::OPACITY));
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());

  // Check that the visibility is correct after the hide animation has finished.
  window->Hide();
  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(window->layer()->visible());
  EXPECT_FALSE(window->IsVisible());
}

}  // namespace wm
