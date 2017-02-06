// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stdint.h>

#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animations.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/completion_event.h"
#include "cc/base/time_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

class LayerTreeHostAnimationTest : public LayerTreeTest {
 public:
  LayerTreeHostAnimationTest()
      : timeline_id_(AnimationIdProvider::NextTimelineId()),
        player_id_(AnimationIdProvider::NextPlayerId()),
        player_child_id_(AnimationIdProvider::NextPlayerId()) {
    timeline_ = AnimationTimeline::Create(timeline_id_);
    player_ = AnimationPlayer::Create(player_id_);
    player_child_ = AnimationPlayer::Create(player_child_id_);

    player_->set_animation_delegate(this);
  }

  void AttachPlayersToTimeline() {
    layer_tree_host()->animation_host()->AddAnimationTimeline(timeline_.get());
    layer_tree_host()->SetElementIdsForTesting();
    timeline_->AttachPlayer(player_.get());
    timeline_->AttachPlayer(player_child_.get());
  }

 protected:
  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<AnimationPlayer> player_;
  scoped_refptr<AnimationPlayer> player_child_;

  const int timeline_id_;
  const int player_id_;
  const int player_child_id_;
};

// Makes sure that SetNeedsAnimate does not cause the CommitRequested() state to
// be set.
class LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested()
      : num_commits_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    // We skip the first commit because its the commit that populates the
    // impl thread with a tree. After the second commit, the test is done.
    if (num_commits_ != 1)
      return;

    layer_tree_host()->SetNeedsAnimate();
    // Right now, CommitRequested is going to be true, because during
    // BeginFrame, we force CommitRequested to true to prevent requests from
    // hitting the impl thread. But, when the next DidCommit happens, we should
    // verify that CommitRequested has gone back to false.
  }

  void DidCommit() override {
    if (!num_commits_) {
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
      layer_tree_host()->SetNeedsAnimate();
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
    }

    // Verifies that the SetNeedsAnimate we made in ::Animate did not
    // trigger CommitRequested.
    EXPECT_FALSE(layer_tree_host()->CommitRequested());
    EndTest();
    num_commits_++;
  }

  void AfterTest() override {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested);

// Trigger a frame with SetNeedsCommit. Then, inside the resulting animate
// callback, request another frame using SetNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where SetNeedsAnimate() is called inside the BeginFrame
// flow.
class LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback()
      : num_begin_frames_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (!num_begin_frames_) {
      layer_tree_host()->SetNeedsAnimate();
      num_begin_frames_++;
      return;
    }
    EndTest();
  }

  void AfterTest() override {}

 private:
  int num_begin_frames_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback);

// Add a layer animation and confirm that
// LayerTreeHostImpl::UpdateAnimationState does get called.
class LayerTreeHostAnimationTestAddAnimation
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddAnimation()
      : update_animation_state_was_called_(false) {}

  void BeginTest() override {
    AttachPlayersToTimeline();
    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddInstantAnimationToMainThreadPlayer(player_.get());
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    EXPECT_FALSE(has_unfinished_animation);
    update_animation_state_was_called_ = true;
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {
    EXPECT_LT(base::TimeTicks(), monotonic_time);

    Animation* animation =
        player_->element_animations()->GetAnimation(TargetProperty::OPACITY);
    if (animation)
      player_->RemoveAnimation(animation->id());

    EndTest();
  }

  void AfterTest() override { EXPECT_TRUE(update_animation_state_was_called_); }

 private:
  bool update_animation_state_was_called_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAddAnimation);

// Add a layer animation to a layer, but continually fail to draw. Confirm that
// after a while, we do eventually force a draw.
class LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws()
      : started_animating_(false) {}

  void BeginTest() override {
    AttachPlayersToTimeline();
    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddAnimationToMainThreadPlayer(player_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    started_animating_ = true;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (started_animating_)
      EndTest();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    return DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
  }

  void AfterTest() override {}

 private:
  bool started_animating_;
};

// Starvation can only be an issue with the MT compositor.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws);

// Ensures that animations eventually get deleted.
class LayerTreeHostAnimationTestAnimationsGetDeleted
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationsGetDeleted()
      : started_animating_(false) {}

  void BeginTest() override {
    AttachPlayersToTimeline();
    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddAnimationToMainThreadPlayer(player_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    bool have_animations = !host_impl->animation_host()
                                ->active_element_animations_for_testing()
                                .empty();
    if (!started_animating_ && have_animations) {
      started_animating_ = true;
      return;
    }

    if (started_animating_ && !have_animations)
      EndTest();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               int group) override {
    // Animations on the impl-side ElementAnimations only get deleted during
    // a commit, so we need to schedule a commit.
    layer_tree_host()->SetNeedsCommit();
  }

  void AfterTest() override {}

 private:
  bool started_animating_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimationsGetDeleted);

// Ensure that an animation's timing function is respected.
class LayerTreeHostAnimationTestAddAnimationWithTimingFunction
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());
    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachPlayersToTimeline();
    player_child_->AttachElement(picture_->element_id());
  }

  void BeginTest() override {
    PostAddAnimationToMainThreadPlayer(player_child_.get());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    // TODO(ajuma): This test only checks the active tree. Add checks for
    // pending tree too.
    if (!host_impl->active_tree()->root_layer_for_testing())
      return;

    // Wait for the commit with the animation to happen.
    if (host_impl->sync_tree()->source_frame_number() != 0)
      return;

    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_child_impl =
        timeline_impl->GetPlayerById(player_child_id_);

    Animation* animation =
        player_child_impl->element_animations()->GetAnimation(
            TargetProperty::OPACITY);

    const FloatAnimationCurve* curve =
        animation->curve()->ToFloatAnimationCurve();
    float start_opacity = curve->GetValue(base::TimeDelta());
    float end_opacity = curve->GetValue(curve->Duration());
    float linearly_interpolated_opacity =
        0.25f * end_opacity + 0.75f * start_opacity;
    base::TimeDelta time = TimeUtil::Scale(curve->Duration(), 0.25f);
    // If the linear timing function associated with this animation was not
    // picked up, then the linearly interpolated opacity would be different
    // because of the default ease timing function.
    EXPECT_FLOAT_EQ(linearly_interpolated_opacity, curve->GetValue(time));

    EndTest();
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddAnimationWithTimingFunction);

// Ensures that main thread animations have their start times synchronized with
// impl thread animations.
class LayerTreeHostAnimationTestSynchronizeAnimationStartTimes
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());

    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachPlayersToTimeline();
    player_child_->set_animation_delegate(this);
    player_child_->AttachElement(picture_->element_id());
  }

  void BeginTest() override {
    PostAddAnimationToMainThreadPlayer(player_child_.get());
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {
    Animation* animation = player_child_->element_animations()->GetAnimation(
        TargetProperty::OPACITY);
    main_start_time_ = animation->start_time();
    player_child_->element_animations()->RemoveAnimation(animation->id());
    EndTest();
  }

  void UpdateAnimationState(LayerTreeHostImpl* impl_host,
                            bool has_unfinished_animation) override {
    scoped_refptr<AnimationTimeline> timeline_impl =
        impl_host->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_child_impl =
        timeline_impl->GetPlayerById(player_child_id_);

    Animation* animation =
        player_child_impl->element_animations()->GetAnimation(
            TargetProperty::OPACITY);
    if (!animation)
      return;

    impl_start_time_ = animation->start_time();
  }

  void AfterTest() override {
    EXPECT_EQ(impl_start_time_, main_start_time_);
    EXPECT_LT(base::TimeTicks(), impl_start_time_);
  }

 private:
  base::TimeTicks main_start_time_;
  base::TimeTicks impl_start_time_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSynchronizeAnimationStartTimes);

// Ensures that notify animation finished is called.
class LayerTreeHostAnimationTestAnimationFinishedEvents
    : public LayerTreeHostAnimationTest {
 public:
  void BeginTest() override {
    AttachPlayersToTimeline();
    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    PostAddInstantAnimationToMainThreadPlayer(player_.get());
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               int group) override {
    Animation* animation =
        player_->element_animations()->GetAnimation(TargetProperty::OPACITY);
    if (animation)
      player_->element_animations()->RemoveAnimation(animation->id());
    EndTest();
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimationFinishedEvents);

// Ensures that when opacity is being animated, this value does not cause the
// subtree to be skipped.
class LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity()
      : update_check_layer_() {}

  void SetupTree() override {
    update_check_layer_ = FakePictureLayer::Create(&client_);
    update_check_layer_->SetOpacity(0.f);
    layer_tree_host()->SetRootLayer(update_check_layer_);
    client_.set_bounds(update_check_layer_->bounds());
    LayerTreeHostAnimationTest::SetupTree();

    AttachPlayersToTimeline();
    player_->AttachElement(update_check_layer_->element_id());
  }

  void BeginTest() override {
    PostAddAnimationToMainThreadPlayer(player_.get());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_impl =
        timeline_impl->GetPlayerById(player_id_);

    Animation* animation_impl = player_impl->element_animations()->GetAnimation(
        TargetProperty::OPACITY);
    player_impl->element_animations()->RemoveAnimation(animation_impl->id());
    EndTest();
  }

  void AfterTest() override {
    // Update() should have been called once, proving that the layer was not
    // skipped.
    EXPECT_EQ(1, update_check_layer_->update_count());

    // clear update_check_layer_ so LayerTreeHost dies.
    update_check_layer_ = NULL;
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> update_check_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity);

// Layers added to tree with existing active animations should have the
// animation correctly recognized.
class LayerTreeHostAnimationTestLayerAddedWithAnimation
    : public LayerTreeHostAnimationTest {
 public:
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      AttachPlayersToTimeline();

      scoped_refptr<Layer> layer = Layer::Create();
      layer->SetElementId(ElementId(42, 0));
      player_->AttachElement(layer->element_id());
      player_->set_animation_delegate(this);

      // Any valid AnimationCurve will do here.
      std::unique_ptr<AnimationCurve> curve(new FakeFloatAnimationCurve());
      std::unique_ptr<Animation> animation(
          Animation::Create(std::move(curve), 1, 1, TargetProperty::OPACITY));
      player_->AddAnimation(std::move(animation));

      // We add the animation *before* attaching the layer to the tree.
      layer_tree_host()->root_layer()->AddChild(layer);
    }
  }

  void AnimateLayers(LayerTreeHostImpl* impl_host,
                     base::TimeTicks monotonic_time) override {
    EndTest();
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestLayerAddedWithAnimation);

class LayerTreeHostAnimationTestCancelAnimateCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCancelAnimateCommit()
      : num_begin_frames_(0), num_commit_calls_(0), num_draw_calls_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    num_begin_frames_++;
    // No-op animate will cancel the commit.
    if (layer_tree_host()->source_frame_number() == 1) {
      EndTest();
      return;
    }
    layer_tree_host()->SetNeedsAnimate();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    num_commit_calls_++;
    if (impl->active_tree()->source_frame_number() > 1)
      FAIL() << "Commit should have been canceled.";
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    num_draw_calls_++;
    if (impl->active_tree()->source_frame_number() > 1)
      FAIL() << "Draw should have been canceled.";
  }

  void AfterTest() override {
    EXPECT_EQ(2, num_begin_frames_);
    EXPECT_EQ(1, num_commit_calls_);
    EXPECT_EQ(1, num_draw_calls_);
  }

 private:
  int num_begin_frames_;
  int num_commit_calls_;
  int num_draw_calls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCancelAnimateCommit);

class LayerTreeHostAnimationTestForceRedraw
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestForceRedraw()
      : num_animate_(0), num_draw_layers_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (++num_animate_ < 2)
      layer_tree_host()->SetNeedsAnimate();
  }

  void UpdateLayerTreeHost() override {
    layer_tree_host()->SetNextCommitForcesRedraw();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (++num_draw_layers_ == 2)
      EndTest();
  }

  void AfterTest() override {
    // The first commit will always draw; make sure the second draw triggered
    // by the animation was not cancelled.
    EXPECT_EQ(2, num_draw_layers_);
    EXPECT_EQ(2, num_animate_);
  }

 private:
  int num_animate_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestForceRedraw);

class LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit()
      : num_animate_(0), num_draw_layers_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (++num_animate_ <= 2) {
      layer_tree_host()->SetNeedsCommit();
      layer_tree_host()->SetNeedsAnimate();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (++num_draw_layers_ == 2)
      EndTest();
  }

  void AfterTest() override {
    // The first commit will always draw; make sure the second draw triggered
    // by the SetNeedsCommit was not cancelled.
    EXPECT_EQ(2, num_draw_layers_);
    EXPECT_GE(num_animate_, 2);
  }

 private:
  int num_animate_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit);

// Animations should not be started when frames are being skipped due to
// checkerboard.
class LayerTreeHostAnimationTestCheckerboardDoesntStartAnimations
    : public LayerTreeHostAnimationTest {
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());
    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachPlayersToTimeline();
    player_child_->AttachElement(picture_->element_id());
    player_child_->set_animation_delegate(this);
  }

  void InitializeSettings(LayerTreeSettings* settings) override {
    // Make sure that drawing many times doesn't cause a checkerboarded
    // animation to start so we avoid flake in this test.
    settings->timeout_and_draw_when_animation_checkerboards = false;
    LayerTreeHostAnimationTest::InitializeSettings(settings);
  }

  void BeginTest() override {
    prevented_draw_ = 0;
    started_times_ = 0;

    PostSetNeedsCommitToMainThread();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    // Don't checkerboard when the first animation wants to start.
    if (host_impl->active_tree()->source_frame_number() < 2)
      return draw_result;
    if (TestEnded())
      return draw_result;
    // Act like there is checkerboard when the second animation wants to draw.
    ++prevented_draw_;
    if (prevented_draw_ > 2)
      EndTest();
    return DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The animation is longer than 1 BeginFrame interval.
        AddOpacityTransitionToPlayer(player_child_.get(), 0.1, 0.2f, 0.8f,
                                     false);
        break;
      case 2:
        // This second animation will not be drawn so it should not start.
        AddAnimatedTransformToPlayer(player_child_.get(), 0.1, 5, 5);
        break;
    }
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {
    if (TestEnded())
      return;
    started_times_++;
  }

  void AfterTest() override {
    // Make sure we tried to draw the second animation but failed.
    EXPECT_LT(0, prevented_draw_);
    // The first animation should be started, but the second should not because
    // of checkerboard.
    EXPECT_EQ(1, started_times_);
  }

  int prevented_draw_;
  int started_times_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestCheckerboardDoesntStartAnimations);

// Verifies that scroll offset animations are only accepted when impl-scrolling
// is supported, and that when scroll offset animations are accepted,
// scroll offset updates are sent back to the main thread.
class LayerTreeHostAnimationTestScrollOffsetChangesArePropagated
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetScrollClipLayerId(layer_tree_host()->root_layer()->id());
    scroll_layer_->SetBounds(gfx::Size(1000, 1000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::ScrollOffset(10, 20));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    AttachPlayersToTimeline();
    player_child_->AttachElement(scroll_layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1: {
        std::unique_ptr<ScrollOffsetAnimationCurve> curve(
            ScrollOffsetAnimationCurve::Create(
                gfx::ScrollOffset(500.f, 550.f),
                CubicBezierTimingFunction::CreatePreset(
                    CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
        std::unique_ptr<Animation> animation(Animation::Create(
            std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
        animation->set_needs_synchronized_start_time(true);
        bool impl_scrolling_supported =
            layer_tree_host()->proxy()->SupportsImplScrolling();
        if (impl_scrolling_supported)
          player_child_->AddAnimation(std::move(animation));
        else
          EndTest();
        break;
      }
      default:
        if (scroll_layer_->scroll_offset().x() > 10 &&
            scroll_layer_->scroll_offset().y() > 20)
          EndTest();
    }
  }

  void AfterTest() override {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestScrollOffsetChangesArePropagated);

// Verifies that when a main thread scrolling reason gets added, the
// notification to takover the animation on the main thread gets sent.
class LayerTreeHostAnimationTestScrollOffsetAnimationTakeover
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationTakeover() {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::ScrollOffset(10, 20));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    AttachPlayersToTimeline();
    player_child_->AttachElement(scroll_layer_->element_id());
    // Allows NotifyAnimationTakeover to get called.
    player_child_->set_animation_delegate(this);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      // Add an update after the first commit to trigger the animation takeover
      // path.
      layer_tree_host()
          ->animation_host()
          ->scroll_offset_animations()
          .AddTakeoverUpdate(scroll_layer_->element_id());
      EXPECT_TRUE(layer_tree_host()
                      ->animation_host()
                      ->scroll_offset_animations()
                      .HasUpdatesForTesting());
    }
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 0) {
      host_impl->animation_host()->ImplOnlyScrollAnimationCreate(
          scroll_layer_->element_id(), gfx::ScrollOffset(650.f, 750.f),
          gfx::ScrollOffset(10, 20));
    }
  }

  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               double animation_start_time,
                               std::unique_ptr<AnimationCurve> curve) override {
    EndTest();
  }

  void AfterTest() override {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationTakeover);

// Verifies that an impl-only scroll offset animation gets updated when the
// scroll offset is adjusted on the main thread.
class LayerTreeHostAnimationTestScrollOffsetAnimationAdjusted
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationAdjusted() {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::ScrollOffset(10, 20));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    AttachPlayersToTimeline();
    player_child_->AttachElement(scroll_layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      // Add an update after the first commit to trigger the animation update
      // path.
      layer_tree_host()
          ->animation_host()
          ->scroll_offset_animations()
          .AddAdjustmentUpdate(scroll_layer_->element_id(),
                               gfx::Vector2dF(100.f, 100.f));
      EXPECT_TRUE(layer_tree_host()
                      ->animation_host()
                      ->scroll_offset_animations()
                      .HasUpdatesForTesting());
    } else if (layer_tree_host()->source_frame_number() == 2) {
      // Verify that the update queue is cleared after the update is applied.
      EXPECT_FALSE(layer_tree_host()
                       ->animation_host()
                       ->scroll_offset_animations()
                       .HasUpdatesForTesting());
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    // Note that the frame number gets incremented after BeginCommitOnThread but
    // before WillCommitCompleteOnThread and CommitCompleteOnThread.
    if (host_impl->sync_tree()->source_frame_number() == 0) {
      // This happens after the impl-only animation is added in
      // WillCommitCompleteOnThread.
      Animation* animation =
          host_impl->animation_host()
              ->GetElementAnimationsForElementId(scroll_layer_->element_id())
              ->GetAnimation(TargetProperty::SCROLL_OFFSET);
      ScrollOffsetAnimationCurve* curve =
          animation->curve()->ToScrollOffsetAnimationCurve();

      // Verifiy the initial and target position before the scroll offset
      // update from MT.
      EXPECT_EQ(Animation::RunState::RUNNING, animation->run_state());
      EXPECT_EQ(gfx::ScrollOffset(10.f, 20.f),
                curve->GetValue(base::TimeDelta()));
      EXPECT_EQ(gfx::ScrollOffset(650.f, 750.f), curve->target_value());
    }
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 0) {
      host_impl->animation_host()->ImplOnlyScrollAnimationCreate(
          scroll_layer_->element_id(), gfx::ScrollOffset(650.f, 750.f),
          gfx::ScrollOffset(10, 20));
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() == 1) {
      Animation* animation =
          host_impl->animation_host()
              ->GetElementAnimationsForElementId(scroll_layer_->element_id())
              ->GetAnimation(TargetProperty::SCROLL_OFFSET);
      ScrollOffsetAnimationCurve* curve =
          animation->curve()->ToScrollOffsetAnimationCurve();
      // Verifiy the initial and target position after the scroll offset
      // update from MT
      EXPECT_EQ(Animation::RunState::STARTING, animation->run_state());
      EXPECT_EQ(gfx::ScrollOffset(110.f, 120.f),
                curve->GetValue(base::TimeDelta()));
      EXPECT_EQ(gfx::ScrollOffset(750.f, 850.f), curve->target_value());

      EndTest();
    }
  }

  void AfterTest() override {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationAdjusted);

// Verifies that when the main thread removes a scroll animation and sets a new
// scroll position, the active tree takes on exactly this new scroll position
// after activation, and the main thread doesn't receive a spurious scroll
// delta.
class LayerTreeHostAnimationTestScrollOffsetAnimationRemoval
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationRemoval()
      : final_postion_(50.0, 100.0) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(&client_);
    scroll_layer_->SetScrollClipLayerId(layer_tree_host()->root_layer()->id());
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    client_.set_bounds(scroll_layer_->bounds());
    scroll_layer_->SetScrollOffset(gfx::ScrollOffset(100.0, 200.0));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    std::unique_ptr<ScrollOffsetAnimationCurve> curve(
        ScrollOffsetAnimationCurve::Create(
            gfx::ScrollOffset(6500.f, 7500.f),
            CubicBezierTimingFunction::CreatePreset(
                CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
    std::unique_ptr<Animation> animation(Animation::Create(
        std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
    animation->set_needs_synchronized_start_time(true);

    AttachPlayersToTimeline();
    player_child_->AttachElement(scroll_layer_->element_id());
    player_child_->AddAnimation(std::move(animation));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        break;
      case 1: {
        Animation* animation =
            player_child_->element_animations()
                ->GetAnimation(TargetProperty::SCROLL_OFFSET);
        player_child_->RemoveAnimation(animation->id());
        scroll_layer_->SetScrollOffset(final_postion_);
        break;
      }
      default:
        EXPECT_EQ(final_postion_, scroll_layer_->scroll_offset());
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->BlockNotifyReadyToActivateForTesting(
        ShouldBlockActivation(host_impl));
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const BeginFrameArgs& args) override {
    host_impl->BlockNotifyReadyToActivateForTesting(
        ShouldBlockActivation(host_impl));
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->pending_tree()->source_frame_number() != 1)
      return;
    LayerImpl* scroll_layer_impl =
        host_impl->pending_tree()->LayerById(scroll_layer_->id());
    EXPECT_EQ(final_postion_, scroll_layer_impl->CurrentScrollOffset());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() != 1)
      return;
    LayerImpl* scroll_layer_impl =
        host_impl->active_tree()->LayerById(scroll_layer_->id());
    EXPECT_EQ(final_postion_, scroll_layer_impl->CurrentScrollOffset());
    EndTest();
  }

  void AfterTest() override {
    EXPECT_EQ(final_postion_, scroll_layer_->scroll_offset());
  }

 private:
  bool ShouldBlockActivation(LayerTreeHostImpl* host_impl) {
    if (!host_impl->pending_tree())
      return false;

    if (!host_impl->active_tree()->root_layer_for_testing())
      return false;

    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_impl =
        timeline_impl->GetPlayerById(player_child_id_);

    LayerImpl* scroll_layer_impl =
        host_impl->active_tree()->LayerById(scroll_layer_->id());
    Animation* animation = player_impl->element_animations()->GetAnimation(
        TargetProperty::SCROLL_OFFSET);

    if (!animation || animation->run_state() != Animation::RUNNING)
      return false;

    // Block activation until the running animation has a chance to produce a
    // scroll delta.
    gfx::Vector2dF scroll_delta = ScrollDelta(scroll_layer_impl);
    if (scroll_delta.x() > 0.f || scroll_delta.y() > 0.f)
      return false;

    return true;
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
  const gfx::ScrollOffset final_postion_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationRemoval);

// When animations are simultaneously added to an existing layer and to a new
// layer, they should start at the same time, even when there's already a
// running animation on the existing layer.
class LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers()
      : frame_count_with_pending_tree_(0) {}

  void BeginTest() override {
    AttachPlayersToTimeline();
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      player_->AttachElement(layer_tree_host()->root_layer()->element_id());
      AddAnimatedTransformToPlayer(player_.get(), 4, 1, 1);
    } else if (layer_tree_host()->source_frame_number() == 2) {
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.f, 0.5f, true);

      scoped_refptr<Layer> layer = Layer::Create();
      layer_tree_host()->root_layer()->AddChild(layer);

      layer_tree_host()->SetElementIdsForTesting();
      layer->SetBounds(gfx::Size(4, 4));

      player_child_->AttachElement(layer->element_id());
      player_child_->set_animation_delegate(this);
      AddOpacityTransitionToPlayer(player_child_.get(), 1, 0.f, 0.5f, true);
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->BlockNotifyReadyToActivateForTesting(true);
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // For the commit that added animations to new and existing layers, keep
    // blocking activation. We want to verify that even with activation blocked,
    // the animation on the layer that's already in the active tree won't get a
    // head start.
    if (host_impl->pending_tree()->source_frame_number() != 2) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const BeginFrameArgs& args) override {
    if (!host_impl->pending_tree() ||
        host_impl->pending_tree()->source_frame_number() != 2)
      return;

    frame_count_with_pending_tree_++;
    if (frame_count_with_pending_tree_ == 2) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_impl =
        timeline_impl->GetPlayerById(player_id_);
    scoped_refptr<AnimationPlayer> player_child_impl =
        timeline_impl->GetPlayerById(player_child_id_);

    // wait for tree activation.
    if (!player_impl->element_animations())
      return;

    Animation* root_animation = player_impl->element_animations()->GetAnimation(
        TargetProperty::OPACITY);
    if (!root_animation || root_animation->run_state() != Animation::RUNNING)
      return;

    Animation* child_animation =
        player_child_impl->element_animations()->GetAnimation(
            TargetProperty::OPACITY);
    EXPECT_EQ(Animation::RUNNING, child_animation->run_state());
    EXPECT_EQ(root_animation->start_time(), child_animation->start_time());
    player_impl->element_animations()->AbortAnimations(TargetProperty::OPACITY);
    player_impl->element_animations()->AbortAnimations(
        TargetProperty::TRANSFORM);
    player_child_impl->element_animations()->AbortAnimations(
        TargetProperty::OPACITY);
    EndTest();
  }

  void AfterTest() override {}

 private:
  int frame_count_with_pending_tree_;
};

// This test blocks activation which is not supported for single thread mode.
MULTI_THREAD_BLOCKNOTIFY_TEST_F(
    LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers);

class LayerTreeHostAnimationTestPendingTreeAnimatesFirstCommit
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(2, 2));
    client_.set_bounds(layer_->bounds());
    // Transform the layer to 4,4 to start.
    gfx::Transform start_transform;
    start_transform.Translate(4.0, 4.0);
    layer_->SetTransform(start_transform);

    layer_tree_host()->root_layer()->AddChild(layer_);
    layer_tree_host()->SetElementIdsForTesting();

    player_->AttachElement(layer_->element_id());

    AttachPlayersToTimeline();
  }

  void BeginTest() override {
    // Add a translate from 6,7 to 8,9.
    TransformOperations start;
    start.AppendTranslate(6.f, 7.f, 0.f);
    TransformOperations end;
    end.AppendTranslate(8.f, 9.f, 0.f);
    AddAnimatedTransformToPlayer(player_.get(), 4.0, start, end);

    PostSetNeedsCommitToMainThread();
  }

  void WillPrepareTiles(LayerTreeHostImpl* host_impl) override {
    if (host_impl->sync_tree()->source_frame_number() != 0)
      return;

    // After checking this on the sync tree, we will activate, which will cause
    // PrepareTiles to happen again (which races with the test exiting).
    if (TestEnded())
      return;

    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_impl =
        timeline_impl->GetPlayerById(player_id_);

    LayerImpl* child = host_impl->sync_tree()->LayerById(layer_->id());
    Animation* animation = player_impl->element_animations()->GetAnimation(
        TargetProperty::TRANSFORM);

    // The animation should be starting for the first frame.
    EXPECT_EQ(Animation::STARTING, animation->run_state());

    // And the transform should be propogated to the sync tree layer, at its
    // starting state which is 6,7.
    gfx::Transform expected_transform;
    expected_transform.Translate(6.0, 7.0);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, child->DrawTransform());
    // And the sync tree layer should know it is animating.
    EXPECT_TRUE(child->screen_space_transform_is_animating());

    player_impl->element_animations()->AbortAnimations(
        TargetProperty::TRANSFORM);
    EndTest();
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<Layer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestPendingTreeAnimatesFirstCommit);

// When a layer with an animation is removed from the tree and later re-added,
// the animation should resume.
class LayerTreeHostAnimationTestAnimatedLayerRemovedAndAdded
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create();
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);

    layer_tree_host()->SetElementIdsForTesting();

    layer_tree_host()->animation_host()->AddAnimationTimeline(timeline_.get());
    timeline_->AttachPlayer(player_.get());
    player_->AttachElement(layer_->element_id());
    DCHECK(player_->element_animations());

    AddOpacityTransitionToPlayer(player_.get(), 10000.0, 0.1f, 0.9f, true);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(
            player_->element_animations()->has_element_in_active_list());
        EXPECT_FALSE(
            player_->element_animations()->has_element_in_pending_list());
        EXPECT_TRUE(layer_tree_host()->animation_host()->NeedsAnimateLayers());
        break;
      case 1:
        layer_->RemoveFromParent();
        EXPECT_FALSE(
            player_->element_animations()->has_element_in_active_list());
        EXPECT_FALSE(
            player_->element_animations()->has_element_in_pending_list());
        EXPECT_FALSE(layer_tree_host()->animation_host()->NeedsAnimateLayers());
        break;
      case 2:
        layer_tree_host()->root_layer()->AddChild(layer_);
        EXPECT_TRUE(
            player_->element_animations()->has_element_in_active_list());
        EXPECT_FALSE(
            player_->element_animations()->has_element_in_pending_list());
        EXPECT_TRUE(layer_tree_host()->animation_host()->NeedsAnimateLayers());
        break;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl->animation_host()->GetTimelineById(timeline_id_);
    scoped_refptr<AnimationPlayer> player_impl =
        timeline_impl->GetPlayerById(player_id_);

    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(
            player_impl->element_animations()->has_element_in_active_list());
        EXPECT_TRUE(host_impl->animation_host()->NeedsAnimateLayers());
        break;
      case 1:
        EXPECT_FALSE(
            player_impl->element_animations()->has_element_in_active_list());
        EXPECT_FALSE(host_impl->animation_host()->NeedsAnimateLayers());
        break;
      case 2:
        EXPECT_TRUE(
            player_impl->element_animations()->has_element_in_active_list());
        EXPECT_TRUE(host_impl->animation_host()->NeedsAnimateLayers());
        EndTest();
        break;
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimatedLayerRemovedAndAdded);

class LayerTreeHostAnimationTestAddAnimationAfterAnimating
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create();
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachPlayersToTimeline();

    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    player_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // First frame: add an animation to the root layer.
        AddAnimatedTransformToPlayer(player_.get(), 0.1, 5, 5);
        break;
      case 2:
        // Second frame: add an animation to the content layer. The root layer
        // animation has caused us to animate already during this frame.
        AddOpacityTransitionToPlayer(player_child_.get(), 0.1, 5, 5, false);
        break;
    }
  }

  void SwapBuffersOnThread(LayerTreeHostImpl* host_impl, bool result) override {
    // After both animations have started, verify that they have valid
    // start times.
    if (host_impl->active_tree()->source_frame_number() < 2)
      return;
    AnimationHost::ElementToAnimationsMap element_animations_copy =
        host_impl->animation_host()->active_element_animations_for_testing();
    EXPECT_EQ(2u, element_animations_copy.size());
    for (auto& it : element_animations_copy) {
      ElementId id = it.first;
      if (id ==
          host_impl->active_tree()->root_layer_for_testing()->element_id()) {
        Animation* anim = it.second->GetAnimation(TargetProperty::TRANSFORM);
        EXPECT_GT((anim->start_time() - base::TimeTicks()).InSecondsF(), 0);
      } else if (id == layer_->element_id()) {
        Animation* anim = it.second->GetAnimation(TargetProperty::OPACITY);
        EXPECT_GT((anim->start_time() - base::TimeTicks()).InSecondsF(), 0);
      }
      EndTest();
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddAnimationAfterAnimating);

class LayerTreeHostAnimationTestRemoveAnimation
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachPlayersToTimeline();

    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    player_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        AddAnimatedTransformToPlayer(player_child_.get(), 1.0, 5, 5);
        break;
      case 2:
        Animation* animation =
            player_child_->element_animations()->GetAnimation(
                TargetProperty::TRANSFORM);
        player_child_->RemoveAnimation(animation->id());
        gfx::Transform transform;
        transform.Translate(10.f, 10.f);
        layer_->SetTransform(transform);

        // Do something that causes property trees to get rebuilt. This is
        // intended to simulate the conditions that caused the bug whose fix
        // this is testing (the test will pass without it but won't test what
        // we want it to). We were updating the wrong transform node at the end
        // of an animation (we were assuming the layer with the finished
        // animation still had its own transform node). But nodes can only get
        // added/deleted when something triggers a rebuild. Adding a layer
        // triggers a rebuild, and since the layer that had an animation before
        // no longer has one, it doesn't get a transform node in the rebuild.
        layer_->AddChild(Layer::Create());
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    LayerImpl* child = host_impl->active_tree()->LayerById(layer_->id());
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is started.
        EXPECT_TRUE(child->screen_space_transform_is_animating());
        break;
      case 2: {
        // The animation is removed, the transform that was set afterward is
        // applied.
        gfx::Transform expected_transform;
        expected_transform.Translate(10.f, 10.f);
        EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                        child->DrawTransform());
        EXPECT_FALSE(child->screen_space_transform_is_animating());
        EndTest();
        break;
      }
      default:
        NOTREACHED();
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestRemoveAnimation);

class LayerTreeHostAnimationTestIsAnimating
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachPlayersToTimeline();
    player_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        AddAnimatedTransformToPlayer(player_.get(), 1.0, 5, 5);
        break;
      case 2:
        Animation* animation = player_->element_animations()->GetAnimation(
            TargetProperty::TRANSFORM);
        player_->RemoveAnimation(animation->id());
        break;
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    LayerImpl* child = host_impl->sync_tree()->LayerById(layer_->id());
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is started.
        EXPECT_TRUE(child->screen_space_transform_is_animating());
        break;
      case 2:
        // The animation is removed/stopped.
        EXPECT_FALSE(child->screen_space_transform_is_animating());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    LayerImpl* child = host_impl->active_tree()->LayerById(layer_->id());
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // No animation yet.
        break;
      case 1:
        // Animation is started.
        EXPECT_TRUE(child->screen_space_transform_is_animating());
        break;
      case 2:
        // The animation is removed/stopped.
        EXPECT_FALSE(child->screen_space_transform_is_animating());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestIsAnimating);

class LayerTreeHostAnimationTestAnimationFinishesDuringCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationFinishesDuringCommit()
      : signalled_(false) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachPlayersToTimeline();

    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    player_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1)
      AddAnimatedTransformToPlayer(player_child_.get(), 0.04, 5, 5);
  }

  void WillCommit() override {
    if (layer_tree_host()->source_frame_number() == 2) {
      // Block until the animation finishes on the compositor thread. Since
      // animations have already been ticked on the main thread, when the commit
      // happens the state on the main thread will be consistent with having a
      // running animation but the state on the compositor thread will be
      // consistent with having only a finished animation.
      completion_.Wait();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        gfx::Transform expected_transform;
        expected_transform.Translate(5.f, 5.f);
        LayerImpl* layer_impl = host_impl->sync_tree()->LayerById(layer_->id());
        EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                        layer_impl->DrawTransform());
        EndTest();
        break;
    }
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    if (host_impl->active_tree()->source_frame_number() == 1 &&
        !has_unfinished_animation && !signalled_) {
      // The animation has finished, so allow the main thread to commit.
      completion_.Signal();
      signalled_ = true;
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
  CompletionEvent completion_;
  bool signalled_;
};

// An animation finishing during commit can only happen when we have a separate
// compositor thread.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimationFinishesDuringCommit);

class LayerTreeHostAnimationTestNotifyAnimationFinished
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestNotifyAnimationFinished()
      : called_animation_started_(false), called_animation_finished_(false) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(picture_->bounds());
    layer_tree_host()->root_layer()->AddChild(picture_);

    AttachPlayersToTimeline();
    player_->AttachElement(picture_->element_id());
    player_->set_animation_delegate(this);
  }

  void BeginTest() override {
    PostAddLongAnimationToMainThreadPlayer(player_.get());
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {
    called_animation_started_ = true;
    layer_tree_host()->AnimateLayers(base::TimeTicks::FromInternalValue(
        std::numeric_limits<int64_t>::max()));
    PostSetNeedsCommitToMainThread();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               int group) override {
    called_animation_finished_ = true;
    EndTest();
  }

  void AfterTest() override {
    EXPECT_TRUE(called_animation_started_);
    EXPECT_TRUE(called_animation_finished_);
  }

 private:
  bool called_animation_started_;
  bool called_animation_finished_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestNotifyAnimationFinished);

// Check that transform sync happens correctly at commit when we remove and add
// a different animation player to an element.
class LayerTreeHostAnimationTestChangeAnimationPlayer
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    AttachPlayersToTimeline();
    timeline_->DetachPlayer(player_child_.get());
    player_->AttachElement(layer_tree_host()->root_layer()->element_id());

    TransformOperations start;
    start.AppendTranslate(5.f, 5.f, 0.f);
    TransformOperations end;
    end.AppendTranslate(5.f, 5.f, 0.f);
    AddAnimatedTransformToPlayer(player_.get(), 1.0, start, end);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    PropertyTrees* property_trees = host_impl->sync_tree()->property_trees();
    TransformNode* node =
        property_trees->transform_tree.Node(host_impl->sync_tree()
                                                ->root_layer_for_testing()
                                                ->transform_tree_index());
    gfx::Transform translate;
    translate.Translate(5, 5);
    switch (host_impl->sync_tree()->source_frame_number()) {
      case 2:
        EXPECT_TRANSFORMATION_MATRIX_EQ(node->data.local, translate);
        EndTest();
        break;
      default:
        break;
    }
  }

  void DidCommit() override { PostSetNeedsCommitToMainThread(); }

  void WillBeginMainFrame() override {
    if (layer_tree_host()->source_frame_number() == 2) {
      // Destroy player.
      timeline_->DetachPlayer(player_.get());
      player_ = nullptr;
      timeline_->AttachPlayer(player_child_.get());
      player_child_->AttachElement(
          layer_tree_host()->root_layer()->element_id());
      AddAnimatedTransformToPlayer(player_child_.get(), 1.0, 10, 10);
      Animation* animation = player_child_->element_animations()->GetAnimation(
          TargetProperty::TRANSFORM);
      animation->set_start_time(base::TimeTicks::Now() +
                                base::TimeDelta::FromSecondsD(1000));
      animation->set_fill_mode(Animation::FillMode::NONE);
    }
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestChangeAnimationPlayer);

// Check that SetTransformIsPotentiallyAnimatingChanged is called
// if we destroy ElementAnimations.
class LayerTreeHostAnimationTestSetPotentiallyAnimatingOnLacDestruction
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    prev_screen_space_transform_is_animating_ = true;
    screen_space_transform_animation_stopped_ = false;

    LayerTreeHostAnimationTest::SetupTree();
    AttachPlayersToTimeline();
    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    AddAnimatedTransformToPlayer(player_.get(), 1.0, 5, 5);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->pending_tree()->source_frame_number() <= 1) {
      EXPECT_TRUE(host_impl->pending_tree()
                      ->root_layer_for_testing()
                      ->screen_space_transform_is_animating());
    } else {
      EXPECT_FALSE(host_impl->pending_tree()
                       ->root_layer_for_testing()
                       ->screen_space_transform_is_animating());
    }
  }

  void DidCommit() override { PostSetNeedsCommitToMainThread(); }

  void UpdateLayerTreeHost() override {
    if (layer_tree_host()->source_frame_number() == 2) {
      // Destroy player.
      timeline_->DetachPlayer(player_.get());
      player_ = nullptr;
    }
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    const bool screen_space_transform_is_animating =
        host_impl->active_tree()
            ->root_layer_for_testing()
            ->screen_space_transform_is_animating();

    // Check that screen_space_transform_is_animating changes only once.
    if (screen_space_transform_is_animating &&
        prev_screen_space_transform_is_animating_)
      EXPECT_FALSE(screen_space_transform_animation_stopped_);
    if (!screen_space_transform_is_animating &&
        prev_screen_space_transform_is_animating_) {
      EXPECT_FALSE(screen_space_transform_animation_stopped_);
      screen_space_transform_animation_stopped_ = true;
    }
    if (!screen_space_transform_is_animating &&
        !prev_screen_space_transform_is_animating_)
      EXPECT_TRUE(screen_space_transform_animation_stopped_);

    prev_screen_space_transform_is_animating_ =
        screen_space_transform_is_animating;

    return draw_result;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() >= 2)
      EndTest();
  }

  void AfterTest() override {
    EXPECT_TRUE(screen_space_transform_animation_stopped_);
  }

  bool prev_screen_space_transform_is_animating_;
  bool screen_space_transform_animation_stopped_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetPotentiallyAnimatingOnLacDestruction);

// Check that we invalidate property trees on AnimationPlayer::SetNeedsCommit.
class LayerTreeHostAnimationTestRebuildPropertyTreesOnAnimationSetNeedsCommit
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(4, 4));
    client_.set_bounds(layer_->bounds());
    layer_tree_host()->root_layer()->AddChild(layer_);

    AttachPlayersToTimeline();

    player_->AttachElement(layer_tree_host()->root_layer()->element_id());
    player_child_->AttachElement(layer_->element_id());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1 ||
        layer_tree_host()->source_frame_number() == 2)
      PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      EXPECT_FALSE(layer_tree_host()->property_trees()->needs_rebuild);
      AddAnimatedTransformToPlayer(player_child_.get(), 1.0, 5, 5);
    }

    EXPECT_TRUE(layer_tree_host()->property_trees()->needs_rebuild);
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() >= 2)
      EndTest();
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
  FakeContentLayerClient client_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestRebuildPropertyTreesOnAnimationSetNeedsCommit);

}  // namespace
}  // namespace cc
