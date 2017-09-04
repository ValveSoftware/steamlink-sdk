// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_thinning.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Mock;
using testing::NiceMock;
using testing::_;

namespace cc {
namespace {

// These constants are hard-coded and should match the values in
// scrollbar_animation_controller_thinning.cc.
const float kIdleThicknessScale = 0.4f;
const float kDefaultMouseMoveDistanceToTriggerAnimation = 25.f;

class MockScrollbarAnimationControllerClient
    : public ScrollbarAnimationControllerClient {
 public:
  explicit MockScrollbarAnimationControllerClient(LayerTreeHostImpl* host_impl)
      : host_impl_(host_impl) {}
  virtual ~MockScrollbarAnimationControllerClient() {}

  void PostDelayedScrollbarAnimationTask(const base::Closure& start_fade,
                                         base::TimeDelta delay) override {
    start_fade_ = start_fade;
    delay_ = delay;
  }
  void SetNeedsRedrawForScrollbarAnimation() override {}
  void SetNeedsAnimateForScrollbarAnimation() override {}
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const override {
    return host_impl_->ScrollbarsFor(scroll_layer_id);
  }
  MOCK_METHOD0(DidChangeScrollbarVisibility, void());

  base::Closure& start_fade() { return start_fade_; }
  base::TimeDelta& delay() { return delay_; }

 private:
  base::Closure start_fade_;
  base::TimeDelta delay_;
  LayerTreeHostImpl* host_impl_;
};

class ScrollbarAnimationControllerThinningTest : public testing::Test {
 public:
  ScrollbarAnimationControllerThinningTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        client_(&host_impl_) {}

 protected:
  const base::TimeDelta kDelayBeforeStarting = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kResizeDelayBeforeStarting =
      base::TimeDelta::FromSeconds(5);
  const base::TimeDelta kFadeDuration = base::TimeDelta::FromSeconds(3);
  const base::TimeDelta kThinningDuration = base::TimeDelta::FromSeconds(2);

  void SetUp() override {
    std::unique_ptr<LayerImpl> scroll_layer =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    std::unique_ptr<LayerImpl> clip =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    clip_layer_ = clip.get();
    scroll_layer->SetScrollClipLayer(clip_layer_->id());
    LayerImpl* scroll_layer_ptr = scroll_layer.get();

    const int kId = 2;
    const int kThumbThickness = 10;
    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;
    const bool kIsOverlayScrollbar = true;

    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
        SolidColorScrollbarLayerImpl::Create(
            host_impl_.active_tree(), kId, HORIZONTAL, kThumbThickness,
            kTrackStart, kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
    scrollbar_layer_ = scrollbar.get();

    scroll_layer->test_properties()->AddChild(std::move(scrollbar));
    clip_layer_->test_properties()->AddChild(std::move(scroll_layer));
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(clip));

    scrollbar_layer_->SetScrollLayerId(scroll_layer_ptr->id());
    scrollbar_layer_->test_properties()->opacity_can_animate = true;
    clip_layer_->SetBounds(gfx::Size(100, 100));
    scroll_layer_ptr->SetBounds(gfx::Size(200, 200));
    host_impl_.active_tree()->BuildLayerListAndPropertyTreesForTesting();

    scrollbar_controller_ = ScrollbarAnimationControllerThinning::Create(
        scroll_layer_ptr->id(), &client_, kDelayBeforeStarting,
        kResizeDelayBeforeStarting, kFadeDuration, kThinningDuration);
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  std::unique_ptr<ScrollbarAnimationControllerThinning> scrollbar_controller_;
  LayerImpl* clip_layer_;
  SolidColorScrollbarLayerImpl* scrollbar_layer_;
  NiceMock<MockScrollbarAnimationControllerClient> client_;
};

// Check initialization of scrollbar. Should start off invisible and thin.
TEST_F(ScrollbarAnimationControllerThinningTest, Idle) {
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Check that scrollbar appears again when the layer becomes scrollable.
TEST_F(ScrollbarAnimationControllerThinningTest, AppearOnResize) {
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // Make the Layer non-scrollable, scrollbar disappears.
  clip_layer_->SetBounds(gfx::Size(200, 200));
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  // Make the layer scrollable, scrollbar appears again.
  clip_layer_->SetBounds(gfx::Size(100, 100));
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
}

// Check that scrollbar disappears when the layer becomes non-scrollable.
TEST_F(ScrollbarAnimationControllerThinningTest, HideOnResize) {
  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer->bounds());

  EXPECT_EQ(HORIZONTAL, scrollbar_layer_->orientation());

  // Shrink along X axis, horizontal scrollbar should appear.
  clip_layer_->SetBounds(gfx::Size(100, 200));
  EXPECT_EQ(gfx::Size(100, 200), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();

  // Shrink along Y axis and expand along X, horizontal scrollbar
  // should disappear.
  clip_layer_->SetBounds(gfx::Size(200, 100));
  EXPECT_EQ(gfx::Size(200, 100), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

// Scroll content. Confirm the scrollbar appears and fades out.
TEST_F(ScrollbarAnimationControllerThinningTest, BasicAppearAndFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scrollbar should be invisible by default.
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  // Scrollbar should appear only on scroll update.
  scrollbar_controller_->DidScrollBegin();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  // Scrollbar should fade out over kFadeDuration.
  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

// Scroll content. Move the mouse near the scrollbar and confirm it becomes
// thick. Ensure it fades out after that.
TEST_F(ScrollbarAnimationControllerThinningTest, MoveNearAndFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse near the scrollbar. This should cancel the currently
  // queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMoveNear(1);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Once the thickening animation is complete, it should enqueue the delayed
  // fade animation.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
}

// Scroll content. Move the mouse over the scrollbar and confirm it becomes
// thick. Ensure it fades out after that.
TEST_F(ScrollbarAnimationControllerThinningTest, MoveOverAndFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar. This should cancel the currently
  // queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMoveNear(0);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Once the thickening animation is complete, it should enqueue the delayed
  // fade animation.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured before the thickening animation doesn't try
// to fade out.
TEST_F(ScrollbarAnimationControllerThinningTest,
       DontFadeWhileCapturedBeforeThick) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and capture it. It should become
  // thick without need for an animation.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->DidMouseDown();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade animation should have been cancelled.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_TRUE(client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured after a thickening animation doesn't try to
// fade out.
TEST_F(ScrollbarAnimationControllerThinningTest, DontFadeWhileCaptured) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and animate it until it's thick.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the scrollbar became thick, it should have queued up a fade.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Make sure capturing the scrollbar stops the fade.
  scrollbar_controller_->DidMouseDown();
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_TRUE(client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar causes it to fade out.
TEST_F(ScrollbarAnimationControllerThinningTest, FadeAfterReleased) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and capture it.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->DidMouseDown();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the scrollbar became thick, it should have queued up a fade.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  scrollbar_controller_->DidMouseUp();
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
}

// Make sure moving near a scrollbar while it's fading out causes it to reset
// the opacity and thicken.
TEST_F(ScrollbarAnimationControllerThinningTest, MoveNearScrollbarWhileFading) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued. Start it.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // Proceed half way through the fade out animation.
  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.5f, scrollbar_layer_->Opacity());

  // Now move the mouse near the scrollbar. It should reset opacity to 1
  // instantly and start animating to thick.
  scrollbar_controller_->DidMouseMoveNear(1);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure capturing a scrollbar while it's fading out causes it to reset the
// opacity and thicken instantly.
TEST_F(ScrollbarAnimationControllerThinningTest, CaptureScrollbarWhileFading) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Move mouse over the scrollbar.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // A fade animation should have been enqueued. Start it.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // Proceed half way through the fade out animation.
  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.5f, scrollbar_layer_->Opacity());

  // Now capture the scrollbar. It should reset opacity to 1 instantly.
  scrollbar_controller_->DidMouseDown();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure we can't capture scrollbar that's completely faded out
TEST_F(ScrollbarAnimationControllerThinningTest, TestCantCaptureWhenFaded) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Move mouse over the scrollbar.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // A fade animation should have been enqueued. Start it.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // Fade the scrollbar out completely.
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  client_.start_fade().Reset();

  // Now try to capture the scrollbar. It shouldn't do anything since it's
  // completely faded out.
  scrollbar_controller_->DidMouseDown();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());

  // Similarly, releasing the scrollbar should have no effect.
  scrollbar_controller_->DidMouseUp();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());
}

// Initiate a scroll when the pointer is already near the scrollbar. It should
// appear thick and remain thick.
TEST_F(ScrollbarAnimationControllerThinningTest, ScrollWithMouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMoveNear(1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;

  // Since the scrollbar isn't visible yet (because we haven't scrolled), we
  // shouldn't have applied the thickening.
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);

  // Now that we've received a scroll, we should be thick without an animation.
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // An animation for the fade should have been enqueued.
  scrollbar_controller_->DidScrollEnd();
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Scrollbar should still be thick, even though the scrollbar fades out.
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer near the scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  scrollbar_controller_->DidMouseMoveNear(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMoveNear(2);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMoveNear(
      kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer over the scrollbar. Make sure it gets thick that it gets
// thin when moved away.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseOver) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves should not change anything.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Moving off the scrollbar but still withing the "near" threshold should do
  // nothing.
  scrollbar_controller_->DidMouseMoveNear(
      kDefaultMouseMoveDistanceToTriggerAnimation - 1.f);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMoveNear(
      kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer over the scrollbar off of it. Make sure the thinning
// animation kicked off in DidMouseMoveOffScrollbar gets overridden by the
// thickening animation in the DidMouseMoveNear call.
TEST_F(ScrollbarAnimationControllerThinningTest,
       MouseNearThenAwayWhileAnimating) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // This is tricky. The DidMouseLeave() is sent before the
  // subsequent DidMouseMoveNear(), if the mouse moves in that direction.
  // This results in the thumb thinning. We want to make sure that when the
  // thumb starts expanding it doesn't first narrow to the idle thinness.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseLeave();
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Let the animation run half of the way through the thinning animation.
  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Now we get a notification for the mouse moving over the scroller. The
  // animation is reset to the thickening direction but we won't start
  // thickening until the new animation catches up to the current thickness.
  scrollbar_controller_->DidMouseMoveNear(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Until we reach the half way point, the animation will have no effect.
  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // We're now at three quarters of the way through so it should now started
  // thickening again.
  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + 3 * (1.0f - kIdleThicknessScale) / 4.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // And all the way to the end.
  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer on the scrollbar, then press it, then away.
// Confirm that the bar gets thick. Then mouse up. Confirm that
// the bar gets thin.
TEST_F(ScrollbarAnimationControllerThinningTest,
       MouseCaptureAndReleaseOutOfBar) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move over the scrollbar.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture
  scrollbar_controller_->DidMouseDown();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Should stay thick for a while.
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);

  // Move outside the "near" threshold. Because the scrollbar is captured it
  // should remain thick.
  scrollbar_controller_->DidMouseMoveNear(
      kDefaultMouseMoveDistanceToTriggerAnimation);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Release.
  scrollbar_controller_->DidMouseUp();

  // Should become thin.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer on the scrollbar, then press it, then away.  Confirm
// that the bar gets thick. Then move point on the scrollbar and mouse up.
// Confirm that the bar stays thick.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseCaptureAndReleaseOnBar) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move over scrollbar.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture. Nothing should change.
  scrollbar_controller_->DidMouseDown();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move away from scrollbar. Nothing should change.
  scrollbar_controller_->DidMouseMoveNear(
      kDefaultMouseMoveDistanceToTriggerAnimation);
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move over scrollbar and release. Since we're near the scrollbar, it should
  // remain thick.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->DidMouseUp();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move mouse on scrollbar and capture then move out of window. Confirm that
// the bar stays thick.
TEST_F(ScrollbarAnimationControllerThinningTest,
       MouseCapturedAndExitWindowFromScrollbar) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse over scrollbar.
  scrollbar_controller_->DidMouseMoveNear(0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture.
  scrollbar_controller_->DidMouseDown();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move out of window. Since the scrollbar is capture, it shouldn't change in
  // any way.
  scrollbar_controller_->DidMouseLeave();
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that the thickening/thinning effects are animated.
TEST_F(ScrollbarAnimationControllerThinningTest, ThicknessAnimated) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse near scrollbar. Test that at half the duration time, the
  // thickness is half way through its animation.
  scrollbar_controller_->DidMouseMoveNear(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move mouse away from scrollbar. Same check.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveNear(
      kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that main thread scroll updates immediatley queue a fade animation
// without requiring a ScrollEnd.
TEST_F(ScrollbarAnimationControllerThinningTest, MainThreadScrollQueuesFade) {
  ASSERT_TRUE(client_.start_fade().is_null());

  // A ScrollUpdate without a ScrollBegin indicates a main thread scroll update
  // so we should schedule a fade animation without waiting for a ScrollEnd
  // (which will never come).
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());

  client_.start_fade().Reset();

  // If we got a ScrollBegin, we shouldn't schedule the fade animation until we
  // get a corresponding ScrollEnd.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_TRUE(client_.start_fade().is_null());
  scrollbar_controller_->DidScrollEnd();
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
}

// Make sure that if the scroll update is as a result of a resize, we use the
// resize delay time instead of the default one.
TEST_F(ScrollbarAnimationControllerThinningTest, ResizeFadeDuration) {
  ASSERT_TRUE(client_.delay().is_zero());

  scrollbar_controller_->DidScrollUpdate(true);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kResizeDelayBeforeStarting, client_.delay());

  client_.delay() = base::TimeDelta();

  // We should use the gesture delay rather than the resize delay if we're in a
  // gesture scroll, even if the resize param is set.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(true);
  scrollbar_controller_->DidScrollEnd();

  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
}

// Tests that the fade effect is animated.
TEST_F(ScrollbarAnimationControllerThinningTest, FadeAnimated) {
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Appearance is instant.
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Test that at half the fade duration time, the opacity is at half.
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.5f, scrollbar_layer_->Opacity());

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

// Tests that the controller tells the client when the scrollbars hide/show.
TEST_F(ScrollbarAnimationControllerThinningTest, NotifyChangedVisibility) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);

  scrollbar_controller_->DidScrollEnd();

  // Play out the fade animation. We shouldn't notify that the scrollbars are
  // hidden until the animation is completly over. We can (but don't have to)
  // notify during the animation that the scrollbars are still visible.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(0);
  ASSERT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.25f, scrollbar_layer_->Opacity());
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  time += kFadeDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  Mock::VerifyAndClearExpectations(&client_);

  // Calling DidScrollUpdate without a begin (i.e. update from commit) should
  // also notify.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);
}

}  // namespace
}  // namespace cc
