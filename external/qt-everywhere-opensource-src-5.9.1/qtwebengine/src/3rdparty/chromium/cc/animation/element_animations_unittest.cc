// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {
namespace {

using base::TimeDelta;
using base::TimeTicks;

static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks::FromInternalValue(seconds *
                                            base::Time::kMicrosecondsPerSecond);
}

// An ElementAnimations cannot be ticked at 0.0, since an animation
// with start time 0.0 is treated as an animation whose start time has
// not yet been set.
const TimeTicks kInitialTickTime = TicksFromSecondsF(1.0);

class ElementAnimationsTest : public AnimationTimelinesTest {
 public:
  ElementAnimationsTest() {}
  ~ElementAnimationsTest() override {}

  std::unique_ptr<AnimationEvents> CreateEventsForTesting() {
    auto mutator_events = host_impl_->CreateEvents();
    return base::WrapUnique(
        static_cast<AnimationEvents*>(mutator_events.release()));
  }
};

// See animation_player_unittest.cc for integration with AnimationPlayer.

TEST_F(ElementAnimationsTest, AttachToLayerInActiveTree) {
  // Set up the layer which is in active tree for main thread and not
  // yet passed onto the impl thread.
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);

  EXPECT_TRUE(client_.IsElementInList(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.IsElementInList(element_id_, ElementListType::PENDING));

  AttachTimelinePlayerLayer();

  EXPECT_TRUE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  PushProperties();

  GetImplTimelineAndPlayerByID();

  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  // Create the layer in the impl active tree.
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  EXPECT_TRUE(
      client_impl_.IsElementInList(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(
      client_impl_.IsElementInList(element_id_, ElementListType::PENDING));

  // kill layer on main thread.
  client_.UnregisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_, player_->element_animations());
  EXPECT_FALSE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  // Sync doesn't detach LayerImpl.
  PushProperties();
  EXPECT_EQ(element_animations_impl_, player_impl_->element_animations());
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  // Kill layer on impl thread in pending tree.
  client_impl_.UnregisterElement(element_id_, ElementListType::PENDING);
  EXPECT_EQ(element_animations_impl_, player_impl_->element_animations());
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Kill layer on impl thread in active tree.
  client_impl_.UnregisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_impl_, player_impl_->element_animations());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Sync doesn't change anything.
  PushProperties();
  EXPECT_EQ(element_animations_impl_, player_impl_->element_animations());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  player_->DetachElement();
  EXPECT_FALSE(player_->element_animations());

  // Release ptrs now to test the order of destruction.
  ReleaseRefPtrs();
}

TEST_F(ElementAnimationsTest, AttachToNotYetCreatedLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);

  PushProperties();
  GetImplTimelineAndPlayerByID();

  // Perform attachment separately.
  player_->AttachElement(element_id_);
  element_animations_ = player_->element_animations();

  EXPECT_FALSE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  PushProperties();
  element_animations_impl_ = player_impl_->element_animations();

  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Create layer.
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());
}

TEST_F(ElementAnimationsTest, AddRemovePlayers) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachElement(element_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->element_animations();
  EXPECT_TRUE(element_animations);

  scoped_refptr<AnimationPlayer> player1 =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  scoped_refptr<AnimationPlayer> player2 =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());

  timeline_->AttachPlayer(player1);
  timeline_->AttachPlayer(player2);

  // Attach players to the same layer.
  player1->AttachElement(element_id_);
  player2->AttachElement(element_id_);

  EXPECT_EQ(element_animations, player1->element_animations());
  EXPECT_EQ(element_animations, player2->element_animations());

  PushProperties();
  GetImplTimelineAndPlayerByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  int list_size_before = 0;
  for (auto& player : element_animations_impl_->players_list()) {
    EXPECT_TRUE(timeline_->GetPlayerById(player.id()));
    ++list_size_before;
  }
  EXPECT_EQ(3, list_size_before);

  player2->DetachElement();
  EXPECT_FALSE(player2->element_animations());
  EXPECT_EQ(element_animations, player_->element_animations());
  EXPECT_EQ(element_animations, player1->element_animations());

  PushProperties();
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());

  int list_size_after = 0;
  for (auto& player : element_animations_impl_->players_list()) {
    EXPECT_TRUE(timeline_->GetPlayerById(player.id()));
    ++list_size_after;
  }
  EXPECT_EQ(2, list_size_after);
}

TEST_F(ElementAnimationsTest, SyncNewAnimation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_impl_->GetAnimation(TargetProperty::OPACITY));

  EXPECT_FALSE(player_->needs_to_start_animations());
  EXPECT_FALSE(player_impl_->needs_to_start_animations());

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);
  EXPECT_TRUE(player_->needs_to_start_animations());

  PushProperties();
  EXPECT_TRUE(player_impl_->needs_to_start_animations());
  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());
}

TEST_F(ElementAnimationsTest,
       SyncScrollOffsetAnimationRespectsHasSetInitialValue) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET));

  EXPECT_FALSE(player_->needs_to_start_animations());
  EXPECT_FALSE(player_impl_->needs_to_start_animations());

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset provider_initial_value(150.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);

  client_impl_.SetScrollOffsetForAnimation(provider_initial_value);

  // Animation with initial value set.
  std::unique_ptr<ScrollOffsetAnimationCurve> curve_fixed(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve_fixed->SetInitialValue(initial_value);
  const int animation1_id = 1;
  std::unique_ptr<Animation> animation_fixed(Animation::Create(
      std::move(curve_fixed), animation1_id, 0, TargetProperty::SCROLL_OFFSET));
  player_->AddAnimation(std::move(animation_fixed));
  PushProperties();
  EXPECT_VECTOR2DF_EQ(initial_value,
                      player_impl_->GetAnimationById(animation1_id)
                          ->curve()
                          ->ToScrollOffsetAnimationCurve()
                          ->GetValue(base::TimeDelta()));

  // Animation without initial value set.
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  const int animation2_id = 2;
  std::unique_ptr<Animation> animation(Animation::Create(
      std::move(curve), animation2_id, 0, TargetProperty::SCROLL_OFFSET));
  player_->AddAnimation(std::move(animation));
  PushProperties();
  EXPECT_VECTOR2DF_EQ(provider_initial_value,
                      player_impl_->GetAnimationById(animation2_id)
                          ->curve()
                          ->ToScrollOffsetAnimationCurve()
                          ->GetValue(base::TimeDelta()));
}

class TestAnimationDelegateThatDestroysPlayer : public TestAnimationDelegate {
 public:
  TestAnimationDelegateThatDestroysPlayer() {}

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {
    TestAnimationDelegate::NotifyAnimationStarted(monotonic_time,
                                                  target_property, group);
    // Detaching player from the timeline ensures that the timeline doesn't hold
    // a reference to the player and the player is destroyed.
    timeline_->DetachPlayer(player_);
  };

  void setTimelineAndPlayer(scoped_refptr<AnimationTimeline> timeline,
                            scoped_refptr<AnimationPlayer> player) {
    timeline_ = timeline;
    player_ = player;
  }

 private:
  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<AnimationPlayer> player_;
};

// Test that we don't crash if a player is deleted while ElementAnimations is
// iterating through the list of players (see crbug.com/631052). This test
// passes if it doesn't crash.
TEST_F(ElementAnimationsTest, AddedPlayerIsDestroyed) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegateThatDestroysPlayer delegate;

  const int player2_id = AnimationIdProvider::NextPlayerId();
  scoped_refptr<AnimationPlayer> player2 = AnimationPlayer::Create(player2_id);
  delegate.setTimelineAndPlayer(timeline_, player2);

  timeline_->AttachPlayer(player2);
  player2->AttachElement(element_id_);
  player2->set_animation_delegate(&delegate);

  int animation_id =
      AddOpacityTransitionToPlayer(player2.get(), 1.0, 0.f, 1.f, false);

  PushProperties();

  scoped_refptr<AnimationPlayer> player2_impl =
      timeline_impl_->GetPlayerById(player2_id);
  DCHECK(player2_impl);

  player2_impl->ActivateAnimations();
  EXPECT_TRUE(player2_impl->GetAnimationById(animation_id));

  scoped_refptr<ElementAnimations> element_animations_impl =
      player2_impl->element_animations();

  element_animations_impl_->Animate(kInitialTickTime);

  auto events = CreateEventsForTesting();
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // The actual detachment happens here, inside the callback
  player2->NotifyAnimationStarted(events->events_[0]);
  EXPECT_TRUE(delegate.started());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST_F(ElementAnimationsTest, DoNotClobberStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_impl_->GetAnimation(TargetProperty::OPACITY));

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  PushProperties();
  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());

  auto events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  player_->NotifyAnimationStarted(events->events_[0]);
  EXPECT_EQ(player_->GetAnimationById(animation_id)->start_time(),
            player_impl_->GetAnimationById(animation_id)->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_EQ(player_->GetAnimationById(animation_id)->start_time(),
            player_impl_->GetAnimationById(animation_id)->start_time());
}

TEST_F(ElementAnimationsTest, UseSpecifiedStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  player_->GetAnimation(TargetProperty::OPACITY)->set_start_time(start_time);

  PushProperties();
  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());

  auto events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  player_->NotifyAnimationStarted(events->events_[0]);

  EXPECT_EQ(start_time, player_->GetAnimationById(animation_id)->start_time());
  EXPECT_EQ(player_->GetAnimationById(animation_id)->start_time(),
            player_impl_->GetAnimationById(animation_id)->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_EQ(start_time, player_->GetAnimationById(animation_id)->start_time());
  EXPECT_EQ(player_->GetAnimationById(animation_id)->start_time(),
            player_impl_->GetAnimationById(animation_id)->start_time());
}

// Tests that animationss activate and deactivate as expected.
TEST_F(ElementAnimationsTest, Activation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  AnimationHost* host = client_.host();
  AnimationHost* host_impl = client_impl_.host();

  auto events = CreateEventsForTesting();

  EXPECT_EQ(1u, host->all_element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->all_element_animations_for_testing().size());

  // Initially, both animationss should be inactive.
  EXPECT_EQ(0u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl->active_element_animations_for_testing().size());

  AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);
  // The main thread animations should now be active.
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());

  PushProperties();
  player_impl_->ActivateAnimations();
  // Both animationss should now be active.
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->active_element_animations_for_testing().size());

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  player_->NotifyAnimationStarted(events->events_[0]);

  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->active_element_animations_for_testing().size());

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_EQ(Animation::FINISHED,
            player_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());

  events = CreateEventsForTesting();

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1500));
  element_animations_impl_->UpdateState(true, events.get());

  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());
  // The impl thread animations should have de-activated.
  EXPECT_EQ(0u, host_impl->active_element_animations_for_testing().size());

  EXPECT_EQ(1u, events->events_.size());
  player_->NotifyAnimationFinished(events->events_[0]);
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1500));
  element_animations_->UpdateState(true, nullptr);

  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            player_->GetAnimation(TargetProperty::OPACITY)->run_state());
  // The main thread animations should have de-activated.
  EXPECT_EQ(0u, host->active_element_animations_for_testing().size());

  PushProperties();
  player_impl_->ActivateAnimations();
  EXPECT_FALSE(player_->has_any_animation());
  EXPECT_FALSE(player_impl_->has_any_animation());
  EXPECT_EQ(0u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl->active_element_animations_for_testing().size());
}

TEST_F(ElementAnimationsTest, SyncPause) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_impl_->GetAnimation(TargetProperty::OPACITY));

  // Two steps, three ranges: [0-1) -> 0.2, [1-2) -> 0.3, [2-3] -> 0.4.
  const double duration = 3.0;
  const int animation_id =
      AddOpacityStepsToPlayer(player_.get(), duration, 0.2f, 0.4f, 2);

  // Set start offset to be at the beginning of the second range.
  player_->GetAnimationById(animation_id)
      ->set_time_offset(TimeDelta::FromSecondsD(1.01));

  PushProperties();
  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());

  TimeTicks time = kInitialTickTime;

  // Start the animations on each animations.
  auto events = CreateEventsForTesting();
  element_animations_impl_->Animate(time);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());

  element_animations_->Animate(time);
  element_animations_->UpdateState(true, nullptr);
  player_->NotifyAnimationStarted(events->events_[0]);

  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimationById(animation_id)->run_state());
  EXPECT_EQ(Animation::RUNNING,
            player_->GetAnimationById(animation_id)->run_state());

  EXPECT_EQ(0.3f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.3f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_EQ(kInitialTickTime,
            player_->GetAnimationById(animation_id)->start_time());
  EXPECT_EQ(kInitialTickTime,
            player_impl_->GetAnimationById(animation_id)->start_time());

  // Pause the animation at the middle of the second range so the offset
  // delays animation until the middle of the third range.
  player_->PauseAnimation(animation_id, 1.5);
  EXPECT_EQ(Animation::PAUSED,
            player_->GetAnimationById(animation_id)->run_state());

  // The pause run state change should make it to the impl thread animations.
  PushProperties();
  player_impl_->ActivateAnimations();

  // Advance time so it stays within the first range.
  time += TimeDelta::FromMilliseconds(10);
  element_animations_->Animate(time);
  element_animations_impl_->Animate(time);

  EXPECT_EQ(Animation::PAUSED,
            player_impl_->GetAnimationById(animation_id)->run_state());

  // Opacity value doesn't depend on time if paused at specified time offset.
  EXPECT_EQ(0.4f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.4f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, DoNotSyncFinishedAnimation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(player_impl_->GetAnimation(TargetProperty::OPACITY));

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  PushProperties();
  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());

  events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // Notify main thread animations that the animation has started.
  player_->NotifyAnimationStarted(events->events_[0]);

  // Complete animation on impl thread.
  events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromSeconds(1));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  player_->NotifyAnimationFinished(events->events_[0]);

  element_animations_->Animate(kInitialTickTime + TimeDelta::FromSeconds(2));
  element_animations_->UpdateState(true, nullptr);

  PushProperties();
  player_impl_->ActivateAnimations();
  EXPECT_FALSE(player_->GetAnimationById(animation_id));
  EXPECT_FALSE(player_impl_->GetAnimationById(animation_id));
}

// Ensure that a finished animation is eventually deleted by both the
// main-thread and the impl-thread animationss.
TEST_F(ElementAnimationsTest, AnimationsAreDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.0f, 1.0f, false);
  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->needs_push_properties());

  PushProperties();
  EXPECT_FALSE(player_->needs_push_properties());

  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(host_impl_->needs_push_properties());

  player_impl_->ActivateAnimations();

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(500));
  element_animations_impl_->UpdateState(true, events.get());

  // There should be a STARTED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  player_->NotifyAnimationStarted(events->events_[0]);

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, nullptr);

  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(host_impl_->needs_push_properties());

  events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(host_impl_->needs_push_properties());

  // There should be a FINISHED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  // Neither animations should have deleted the animation yet.
  EXPECT_TRUE(player_->GetAnimation(TargetProperty::OPACITY));
  EXPECT_TRUE(player_impl_->GetAnimation(TargetProperty::OPACITY));

  player_->NotifyAnimationFinished(events->events_[0]);

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(3000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();

  // Both animationss should now have deleted the animation. The impl animations
  // should have deleted the animation even though activation has not occurred,
  // since the animation was already waiting for deletion when
  // PushPropertiesTo was called.
  EXPECT_FALSE(player_->has_any_animation());
  EXPECT_FALSE(player_impl_->has_any_animation());
}

// Tests that transitioning opacity from 0 to 1 works as expected.

static std::unique_ptr<Animation> CreateAnimation(
    std::unique_ptr<AnimationCurve> curve,
    int group_id,
    TargetProperty::Type property) {
  return Animation::Create(std::move(curve), 0, group_id, property);
}

static const AnimationEvent* GetMostRecentPropertyUpdateEvent(
    const AnimationEvents* events) {
  const AnimationEvent* event = 0;
  for (size_t i = 0; i < events->events_.size(); ++i)
    if (events->events_[i].type == AnimationEvent::PROPERTY_UPDATE)
      event = &events->events_[i];

  return event;
}

TEST_F(ElementAnimationsTest, TrivialTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  EXPECT_FALSE(player_->needs_to_start_animations());
  player_->AddAnimation(std::move(to_add));
  EXPECT_TRUE(player_->needs_to_start_animations());
  element_animations_->Animate(kInitialTickTime);
  EXPECT_FALSE(player_->needs_to_start_animations());
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

TEST_F(ElementAnimationsTest, FilterTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(1.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                            end_filters, nullptr));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::FILTER));
  player_->AddAnimation(std::move(animation));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(start_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(1u,
            client_.GetFilters(element_id_, ElementListType::ACTIVE).size());
  EXPECT_EQ(FilterOperation::CreateBrightnessFilter(1.5f),
            client_.GetFilters(element_id_, ElementListType::ACTIVE).at(0));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(end_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_needs_synchronized_start_time(true);
  player_->AddAnimation(std::move(animation));

  client_impl_.SetScrollOffsetForAnimation(initial_value);
  PushProperties();
  player_impl_->ActivateAnimations();
  EXPECT_TRUE(player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration = player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)
                           ->curve()
                           ->Duration();
  EXPECT_EQ(duration, player_->GetAnimation(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(player_impl_->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  // Scroll offset animations should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  player_->NotifyAnimationStarted(events->events_[0]);
  element_animations_->Animate(kInitialTickTime + duration / 2);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime + duration / 2);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_impl_->Animate(kInitialTickTime + duration);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_impl_->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_->Animate(kInitialTickTime + duration);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransitionOnImplOnly) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  double duration_in_seconds = curve->Duration().InSecondsF();

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_is_impl_only(true);
  player_impl_->AddAnimation(std::move(animation));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(player_impl_->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  // Scroll offset animations should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  TimeDelta duration = TimeDelta::FromMicroseconds(
      duration_in_seconds * base::Time::kMicrosecondsPerSecond);

  element_animations_impl_->Animate(kInitialTickTime + duration / 2);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_impl_->Animate(kInitialTickTime + duration);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_impl_->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

// This test verifies that if an animation is added after a layer is animated,
// it doesn't get promoted to be in the RUNNING state. This prevents cases where
// a start time gets set on an animation using the stale value of
// last_tick_time_.
TEST_F(ElementAnimationsTest, UpdateStateWithoutAnimate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  // Add first scroll offset animation.
  AddScrollOffsetAnimationToPlayer(player_impl_.get(),
                                   gfx::ScrollOffset(100.f, 300.f),
                                   gfx::ScrollOffset(100.f, 200.f), true);

  // Calling UpdateState after Animate should promote the animation to running
  // state.
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(
      Animation::RUNNING,
      player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)->run_state());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1500));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(
      Animation::WAITING_FOR_DELETION,
      player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)->run_state());

  // Add second scroll offset animation.
  AddScrollOffsetAnimationToPlayer(player_impl_.get(),
                                   gfx::ScrollOffset(100.f, 200.f),
                                   gfx::ScrollOffset(100.f, 100.f), true);

  // Calling UpdateState without Animate should NOT promote the animation to
  // running state.
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(
      Animation::WAITING_FOR_TARGET_AVAILABILITY,
      player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)->run_state());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  EXPECT_EQ(
      Animation::RUNNING,
      player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)->run_state());
  EXPECT_VECTOR2DF_EQ(
      gfx::ScrollOffset(100.f, 200.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
}

// Ensure that when the impl animations doesn't have a value provider,
// the main-thread animations's value provider is used to obtain the intial
// scroll offset.
TEST_F(ElementAnimationsTest, ScrollOffsetTransitionNoImplProvider) {
  CreateTestLayer(false, false);
  CreateTestImplLayer(ElementListType::PENDING);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(500.f, 100.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_needs_synchronized_start_time(true);
  player_->AddAnimation(std::move(animation));

  client_.SetScrollOffsetForAnimation(initial_value);
  PushProperties();
  player_impl_->ActivateAnimations();
  EXPECT_TRUE(player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration = player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)
                           ->curve()
                           ->Duration();
  EXPECT_EQ(duration, player_->GetAnimation(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, nullptr);

  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(gfx::ScrollOffset(), client_impl_.GetScrollOffset(
                                     element_id_, ElementListType::PENDING));

  element_animations_impl_->Animate(kInitialTickTime);

  EXPECT_TRUE(player_impl_->HasActiveAnimation());
  EXPECT_EQ(initial_value, client_impl_.GetScrollOffset(
                               element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  element_animations_impl_->UpdateState(true, events.get());
  DCHECK_EQ(1UL, events->events_.size());

  // Scroll offset animations should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  player_->NotifyAnimationStarted(events->events_[0]);
  element_animations_->Animate(kInitialTickTime + duration / 2);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime + duration / 2);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::PENDING));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_impl_->Animate(kInitialTickTime + duration);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::PENDING));
  EXPECT_FALSE(player_impl_->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  element_animations_->Animate(kInitialTickTime + duration);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

TEST_F(ElementAnimationsTest, ScrollOffsetRemovalClearsScrollDelta) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  // First test the 1-argument version of RemoveAnimation.
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  int animation_id = 1;
  std::unique_ptr<Animation> animation(Animation::Create(
      std::move(curve), animation_id, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_needs_synchronized_start_time(true);
  player_->AddAnimation(std::move(animation));
  PushProperties();
  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  player_->RemoveAnimation(animation_id);
  EXPECT_TRUE(element_animations_->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_TRUE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  // Now, test the 2-argument version of RemoveAnimation.
  curve = ScrollOffsetAnimationCurve::Create(
      target_value, CubicBezierTimingFunction::CreatePreset(
                        CubicBezierTimingFunction::EaseType::EASE_IN_OUT));
  animation = Animation::Create(std::move(curve), animation_id, 0,
                                TargetProperty::SCROLL_OFFSET);
  animation->set_needs_synchronized_start_time(true);
  player_->AddAnimation(std::move(animation));
  PushProperties();
  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  player_->RemoveAnimation(animation_id);
  EXPECT_TRUE(element_animations_->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_TRUE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  // Check that removing non-scroll-offset animations does not cause
  // scroll_offset_animation_was_interrupted() to get set.
  animation_id = AddAnimatedTransformToPlayer(player_.get(), 1.0, 1, 2);
  PushProperties();
  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  player_->RemoveAnimation(animation_id);
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  animation_id = AddAnimatedFilterToPlayer(player_.get(), 1.0, 0.1f, 0.2f);
  PushProperties();
  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());

  player_->RemoveAnimation(animation_id);
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(element_animations_->scroll_offset_animation_was_interrupted());

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(
      element_animations_impl_->scroll_offset_animation_was_interrupted());
}

// Tests that impl-only animations lead to start and finished notifications
// on the impl thread animations's animation delegate.
TEST_F(ElementAnimationsTest,
       NotificationsForImplOnlyAnimationsAreSentToImplThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  TestAnimationDelegate delegate;
  player_impl_->set_animation_delegate(&delegate);

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  TimeDelta duration = curve->Duration();
  std::unique_ptr<Animation> to_add(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  to_add->set_is_impl_only(true);
  player_impl_->AddAnimation(std::move(to_add));

  EXPECT_FALSE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime + duration);
  EXPECT_EQ(duration, player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());
  element_animations_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_TRUE(delegate.finished());
}

// Tests that specified start times are sent to the main thread delegate
TEST_F(ElementAnimationsTest, SpecifiedStartTimesAreSentToMainThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  player_->GetAnimation(TargetProperty::OPACITY)->set_start_time(start_time);

  PushProperties();
  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());

  auto events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  player_->NotifyAnimationStarted(events->events_[0]);

  // Validate start time on the main thread delegate.
  EXPECT_EQ(start_time, delegate.start_time());
}

// Tests animations that are waiting for a synchronized start time do not
// finish.
TEST_F(ElementAnimationsTest,
       AnimationsWaitingForStartTimeDoNotFinishIfTheyOutwaitTheirFinish) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);

  // We should pause at the first keyframe indefinitely waiting for that
  // animation to start.
  player_->AddAnimation(std::move(to_add));
  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Send the synchronized start time.
  player_->NotifyAnimationStarted(AnimationEvent(
      AnimationEvent::STARTED, ElementId(), 1, TargetProperty::OPACITY,
      kInitialTickTime + TimeDelta::FromMilliseconds(2000)));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(5000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST_F(ElementAnimationsTest, TrivialQueuing) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(player_->needs_to_start_animations());

  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));

  EXPECT_TRUE(player_->needs_to_start_animations());

  element_animations_->Animate(kInitialTickTime);

  // The second animation still needs to be started.
  EXPECT_TRUE(player_->needs_to_start_animations());

  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(player_->needs_to_start_animations());
  element_animations_->UpdateState(true, events.get());
  EXPECT_FALSE(player_->needs_to_start_animations());

  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST_F(ElementAnimationsTest, Interrupt) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  player_->AbortAnimations(TargetProperty::OPACITY, false);
  player_->AddAnimation(std::move(to_add));

  // Since the previous animation was aborted, the new animation should start
  // right in this call to animate.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1500));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is
// free.
TEST_F(ElementAnimationsTest, ScheduleTogetherWhenAPropertyIsBlocked) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 2,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(player_->HasActiveAnimation());
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The float animation should have started at time 1 and should be done.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and
// another animation queued to start when the shorter animation finishes (should
// wait for both to finish).
TEST_F(ElementAnimationsTest, ScheduleTogetherWithAnAnimWaiting) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2)), 1,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));

  // Animations with id 1 should both start now.
  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The opacity animation should have finished at time 1, but the group
  // of animations with id 1 don't finish until time 2 because of the length
  // of the transform animation.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  element_animations_->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The second opacity animation should start at time 2 and should be done by
  // time 3.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(3000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST_F(ElementAnimationsTest, TrivialLooping) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(3);
  player_->AddAnimation(std::move(to_add));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1250));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1750));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2250));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2750));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(3000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_FALSE(player_->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Just be extra sure.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(4000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST_F(ElementAnimationsTest, InfiniteLooping) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(-1);
  player_->AddAnimation(std::move(to_add));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1250));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1750));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1073741824250));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1073741824750));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetAnimation(TargetProperty::OPACITY));
  player_->GetAnimation(TargetProperty::OPACITY)
      ->SetRunState(Animation::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(750));
  EXPECT_FALSE(player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that pausing and resuming work as expected.
TEST_F(ElementAnimationsTest, PauseResume) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetAnimation(TargetProperty::OPACITY));
  player_->GetAnimation(TargetProperty::OPACITY)
      ->SetRunState(Animation::PAUSED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(500));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1024000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetAnimation(TargetProperty::OPACITY));
  player_->GetAnimation(TargetProperty::OPACITY)
      ->SetRunState(Animation::RUNNING,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1024250));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1024500));
  element_animations_->UpdateState(true, events.get());
  EXPECT_FALSE(player_->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AbortAGroupedAnimation) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  const int animation_id = 2;
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1, 1,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      animation_id, 1, TargetProperty::OPACITY));
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.75f)),
      3, 2, TargetProperty::OPACITY));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetAnimationById(animation_id));
  player_->GetAnimationById(animation_id)
      ->SetRunState(Animation::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(!player_->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushUpdatesWhenSynchronizedStartTimeNeeded) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      0, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);
  player_->AddAnimation(std::move(to_add));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->HasActiveAnimation());
  Animation* active_animation = player_->GetAnimation(TargetProperty::OPACITY);
  EXPECT_TRUE(active_animation);
  EXPECT_TRUE(active_animation->needs_synchronized_start_time());

  EXPECT_TRUE(player_->needs_push_properties());
  PushProperties();
  player_impl_->ActivateAnimations();

  active_animation = player_impl_->GetAnimation(TargetProperty::OPACITY);
  EXPECT_TRUE(active_animation);
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            active_animation->run_state());
}

// Tests that skipping a call to UpdateState works as expected.
TEST_F(ElementAnimationsTest, SkipUpdateState) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<Animation> first_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  first_animation->set_is_controlling_instance_for_test(true);
  player_->AddAnimation(std::move(first_animation));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, events.get());

  std::unique_ptr<Animation> second_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));
  second_animation->set_is_controlling_instance_for_test(true);
  player_->AddAnimation(std::move(second_animation));

  // Animate but don't UpdateState.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  events = CreateEventsForTesting();
  element_animations_->UpdateState(true, events.get());

  // Should have one STARTED event and one FINISHED event.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_NE(events->events_[0].type, events->events_[1].type);

  // The float transition should still be at its starting point.
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(3000));
  element_animations_->UpdateState(true, events.get());

  // The float tranisition should now be done.
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->HasActiveAnimation());
}

// Tests that an animation animations with only a pending observer gets ticked
// but doesn't progress animations past the STARTING state.
TEST_F(ElementAnimationsTest, InactiveObserverGetsTicked) {
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int id = 1;
  player_impl_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.5f, 1.f)),
      id, TargetProperty::OPACITY));

  // Without an observer, the animation shouldn't progress to the STARTING
  // state.
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());

  CreateTestImplLayer(ElementListType::PENDING);

  // With only a pending observer, the animation should progress to the
  // STARTING state and get ticked at its starting point, but should not
  // progress to RUNNING.
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::STARTING,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  // Even when already in the STARTING state, the animation should stay
  // there, and shouldn't be ticked past its starting point.
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::STARTING,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  // Now that an active observer has been added, the animation should still
  // initially tick at its starting point, but should now progress to RUNNING.
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(3000));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The animation should now tick past its starting point.
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(3500));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TransformAnimationBounds) {
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendTranslate(10.0, 15.0, 0.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1, nullptr));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  player_impl_->AddAnimation(std::move(animation));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations2, nullptr));
  operations2.AppendScale(2.0, 3.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  animation =
      Animation::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);
  player_impl_->AddAnimation(std::move(animation));

  gfx::BoxF box(1.f, 2.f, -1.f, 3.f, 4.f, 5.f);
  gfx::BoxF bounds;

  EXPECT_TRUE(player_impl_->TransformAnimationBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -4.f, 13.f, 19.f, 20.f).ToString(),
            bounds.ToString());

  player_impl_->GetAnimationById(1)->SetRunState(Animation::FINISHED,
                                                 TicksFromSecondsF(0.0));

  // Only the unfinished animation should affect the animated bounds.
  EXPECT_TRUE(player_impl_->TransformAnimationBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -4.f, 7.f, 16.f, 20.f).ToString(),
            bounds.ToString());

  player_impl_->GetAnimationById(2)->SetRunState(Animation::FINISHED,
                                                 TicksFromSecondsF(0.0));

  // There are no longer any running animations.
  EXPECT_FALSE(player_impl_->HasTransformAnimationThatInflatesBounds());

  // Add an animation whose bounds we don't yet support computing.
  std::unique_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations3;
  gfx::Transform transform3;
  transform3.Scale3d(1.0, 2.0, 3.0);
  curve3->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3, nullptr));
  operations3.AppendMatrix(transform3);
  curve3->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));
  animation =
      Animation::Create(std::move(curve3), 3, 3, TargetProperty::TRANSFORM);
  player_impl_->AddAnimation(std::move(animation));
  EXPECT_FALSE(player_impl_->TransformAnimationBoundsForBox(box, &bounds));
}

// Tests that AbortAnimations aborts all animations targeting the specified
// property.
TEST_F(ElementAnimationsTest, AbortAnimations) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Start with several animations, and allow some of them to reach the finished
  // state.
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1, 1,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, 2, TargetProperty::OPACITY));
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 3, 3,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 4, 4,
      TargetProperty::TRANSFORM));
  player_->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      5, 5, TargetProperty::OPACITY));

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, nullptr);
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, nullptr);

  EXPECT_EQ(Animation::FINISHED, player_->GetAnimationById(1)->run_state());
  EXPECT_EQ(Animation::FINISHED, player_->GetAnimationById(2)->run_state());
  EXPECT_EQ(Animation::RUNNING, player_->GetAnimationById(3)->run_state());
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_->GetAnimationById(4)->run_state());
  EXPECT_EQ(Animation::RUNNING, player_->GetAnimationById(5)->run_state());

  player_->AbortAnimations(TargetProperty::TRANSFORM, false);

  // Only un-finished TRANSFORM animations should have been aborted.
  EXPECT_EQ(Animation::FINISHED, player_->GetAnimationById(1)->run_state());
  EXPECT_EQ(Animation::FINISHED, player_->GetAnimationById(2)->run_state());
  EXPECT_EQ(Animation::ABORTED, player_->GetAnimationById(3)->run_state());
  EXPECT_EQ(Animation::ABORTED, player_->GetAnimationById(4)->run_state());
  EXPECT_EQ(Animation::RUNNING, player_->GetAnimationById(5)->run_state());
}

// An animation aborted on the main thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, MainThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 1.f, false);
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();

  player_impl_->ActivateAnimations();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_FALSE(host_->needs_push_properties());

  player_->AbortAnimations(TargetProperty::OPACITY, false);
  EXPECT_EQ(Animation::ABORTED,
            player_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(host_->needs_push_properties());

  element_animations_->Animate(kInitialTickTime);
  element_animations_->UpdateState(true, nullptr);
  EXPECT_EQ(Animation::ABORTED,
            player_->GetAnimation(TargetProperty::OPACITY)->run_state());

  EXPECT_TRUE(player_->needs_push_properties());
  EXPECT_TRUE(player_->needs_push_properties());
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(player_->needs_push_properties());

  EXPECT_FALSE(player_->GetAnimationById(animation_id));
  EXPECT_FALSE(player_impl_->GetAnimationById(animation_id));
}

// An animation aborted on the impl thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, ImplThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 1.f, false);

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));

  player_impl_->AbortAnimations(TargetProperty::OPACITY, false);
  EXPECT_EQ(Animation::ABORTED,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_TRUE(player_impl_->needs_push_properties());

  auto events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[0].type);
  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            player_impl_->GetAnimation(TargetProperty::OPACITY)->run_state());

  player_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_EQ(Animation::ABORTED,
            player_->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(delegate.aborted());

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(500));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->needs_push_properties());
  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            player_->GetAnimation(TargetProperty::OPACITY)->run_state());

  PushProperties();

  player_impl_->ActivateAnimations();
  EXPECT_FALSE(player_->GetAnimationById(animation_id));
  EXPECT_FALSE(player_impl_->GetAnimationById(animation_id));
}

// Test that an impl-only scroll offset animation that needs to be completed on
// the main thread gets deleted.
TEST_F(ElementAnimationsTest, ImplThreadTakeoverAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegate delegate_impl;
  player_impl_->set_animation_delegate(&delegate_impl);
  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  // Add impl-only scroll offset animation.
  const int animation_id = 1;
  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  std::unique_ptr<Animation> animation(Animation::Create(
      std::move(curve), animation_id, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_start_time(TicksFromSecondsF(123));
  animation->set_is_impl_only(true);
  player_impl_->AddAnimation(std::move(animation));

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));

  player_impl_->AbortAnimations(TargetProperty::SCROLL_OFFSET, true);
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(
      Animation::ABORTED_BUT_NEEDS_COMPLETION,
      player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)->run_state());

  auto events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(delegate_impl.finished());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::TAKEOVER, events->events_[0].type);
  EXPECT_EQ(123, events->events_[0].animation_start_time);
  EXPECT_EQ(
      target_value,
      events->events_[0].curve->ToScrollOffsetAnimationCurve()->target_value());
  EXPECT_EQ(
      Animation::WAITING_FOR_DELETION,
      player_impl_->GetAnimation(TargetProperty::SCROLL_OFFSET)->run_state());

  // MT receives the event to take over.
  player_->NotifyAnimationTakeover(events->events_[0]);
  EXPECT_TRUE(delegate.takeover());

  // AnimationPlayer::NotifyAnimationTakeover requests SetNeedsPushProperties
  // to purge CT animations marked for deletion.
  EXPECT_TRUE(player_->needs_push_properties());

  // ElementAnimations::PurgeAnimationsMarkedForDeletion call happens only in
  // ElementAnimations::PushPropertiesTo.
  PushProperties();

  player_impl_->ActivateAnimations();
  EXPECT_FALSE(player_->GetAnimationById(animation_id));
  EXPECT_FALSE(player_impl_->GetAnimationById(animation_id));
}

// Ensure that we only generate FINISHED events for animations in a group
// once all animations in that group are finished.
TEST_F(ElementAnimationsTest, FinishedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int group_id = 1;

  // Add two animations with the same group id but different durations.
  std::unique_ptr<Animation> first_animation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 1,
      group_id, TargetProperty::TRANSFORM));
  first_animation->set_is_controlling_instance_for_test(true);
  player_impl_->AddAnimation(std::move(first_animation));

  std::unique_ptr<Animation> second_animation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, group_id, TargetProperty::OPACITY));
  second_animation->set_is_controlling_instance_for_test(true);
  player_impl_->AddAnimation(std::move(second_animation));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());

  // The opacity animation should be finished, but should not have generated
  // a FINISHED event yet.
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::FINISHED,
            player_impl_->GetAnimationById(2)->run_state());
  EXPECT_EQ(Animation::RUNNING, player_impl_->GetAnimationById(1)->run_state());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  // Both animations should have generated FINISHED events.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[1].type);
}

// Ensure that when a group has a mix of aborted and finished animations,
// we generate a FINISHED event for the finished animation and an ABORTED
// event for the aborted animation.
TEST_F(ElementAnimationsTest, FinishedAndAbortedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  // Add two animations with the same group id.
  std::unique_ptr<Animation> first_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1,
      TargetProperty::TRANSFORM));
  first_animation->set_is_controlling_instance_for_test(true);
  player_impl_->AddAnimation(std::move(first_animation));

  std::unique_ptr<Animation> second_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  second_animation->set_is_controlling_instance_for_test(true);
  player_impl_->AddAnimation(std::move(second_animation));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  player_impl_->AbortAnimations(TargetProperty::OPACITY, false);

  events = CreateEventsForTesting();
  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());

  // We should have exactly 2 events: a FINISHED event for the tranform
  // animation, and an ABORTED event for the opacity animation.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(TargetProperty::TRANSFORM, events->events_[0].target_property);
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[1].type);
  EXPECT_EQ(TargetProperty::OPACITY, events->events_[1].target_property);
}

TEST_F(ElementAnimationsTest, HasOnlyTranslationTransforms) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));

  player_impl_->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  // Opacity animations aren't non-translation transforms.
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendTranslate(10.0, 15.0, 0.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1, nullptr));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve1), 2, 2, TargetProperty::TRANSFORM));
  player_impl_->AddAnimation(std::move(animation));

  // The only transform animation we've added is a translation.
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations2, nullptr));
  operations2.AppendScale(2.0, 3.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  animation =
      Animation::Create(std::move(curve2), 3, 3, TargetProperty::TRANSFORM);
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  // A scale animation is not a translation.
  EXPECT_FALSE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));

  player_impl_->ActivateAnimations();
  EXPECT_FALSE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_FALSE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));

  player_impl_->GetAnimationById(3)->set_affects_pending_elements(false);
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_FALSE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));

  player_impl_->GetAnimationById(3)->SetRunState(Animation::FINISHED,
                                                 TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // HasOnlyTranslationTransforms.
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_TRUE(
      player_impl_->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AnimationStartScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  operations1.AppendScale(2.0, 3.0, 4.0);
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  TransformOperations operations2;
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));
  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  float start_scale = 0.f;
  EXPECT_TRUE(player_impl_->AnimationStartScale(ElementListType::PENDING,
                                                &start_scale));
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(
      player_impl_->AnimationStartScale(ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(0.f, start_scale);

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(player_impl_->AnimationStartScale(ElementListType::PENDING,
                                                &start_scale));
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(
      player_impl_->AnimationStartScale(ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(4.f, start_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations3;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3, nullptr));
  operations3.AppendScale(6.0, 5.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));

  player_impl_->RemoveAnimation(1);
  animation =
      Animation::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);

  // Reverse Direction
  animation->set_direction(Animation::Direction::REVERSE);
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations4;
  operations4.AppendScale(5.0, 3.0, 1.0);
  curve3->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations4, nullptr));
  TransformOperations operations5;
  curve3->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations5, nullptr));

  animation =
      Animation::Create(std::move(curve3), 3, 3, TargetProperty::TRANSFORM);
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  EXPECT_TRUE(player_impl_->AnimationStartScale(ElementListType::PENDING,
                                                &start_scale));
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(
      player_impl_->AnimationStartScale(ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(0.f, start_scale);

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(player_impl_->AnimationStartScale(ElementListType::PENDING,
                                                &start_scale));
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(
      player_impl_->AnimationStartScale(ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(6.f, start_scale);

  player_impl_->GetAnimationById(2)->SetRunState(Animation::FINISHED,
                                                 TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // AnimationStartScale.
  EXPECT_TRUE(player_impl_->AnimationStartScale(ElementListType::PENDING,
                                                &start_scale));
  EXPECT_EQ(5.f, start_scale);
  EXPECT_TRUE(
      player_impl_->AnimationStartScale(ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(5.f, start_scale);
}

TEST_F(ElementAnimationsTest, MaximumTargetScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  float max_scale = 0.f;
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(0.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(0.f, max_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendScale(2.0, 3.0, 4.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1, nullptr));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(0.f, max_scale);

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations2, nullptr));
  operations2.AppendScale(6.0, 5.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  animation =
      Animation::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations3;
  curve3->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3, nullptr));
  operations3.AppendPerspective(6.0);
  curve3->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));

  animation =
      Animation::Create(std::move(curve3), 3, 3, TargetProperty::TRANSFORM);
  animation->set_affects_active_elements(false);
  player_impl_->AddAnimation(std::move(animation));

  EXPECT_FALSE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  player_impl_->ActivateAnimations();
  EXPECT_FALSE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_FALSE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));

  player_impl_->GetAnimationById(3)->SetRunState(Animation::FINISHED,
                                                 TicksFromSecondsF(0.0));
  player_impl_->GetAnimationById(2)->SetRunState(Animation::FINISHED,
                                                 TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // MaximumTargetScale.
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);
}

TEST_F(ElementAnimationsTest, MaximumTargetScaleWithDirection) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendScale(1.0, 2.0, 3.0);
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  TransformOperations operations2;
  operations2.AppendScale(4.0, 5.0, 6.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  std::unique_ptr<Animation> animation_owned(
      Animation::Create(std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  Animation* animation = animation_owned.get();
  player_impl_->AddAnimation(std::move(animation_owned));

  float max_scale = 0.f;

  EXPECT_GT(animation->playback_rate(), 0.0);

  // NORMAL direction with positive playback rate.
  animation->set_direction(Animation::Direction::NORMAL);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // ALTERNATE direction with positive playback rate.
  animation->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // REVERSE direction with positive playback rate.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // ALTERNATE reverse direction.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  animation->set_playback_rate(-1.0);

  // NORMAL direction with negative playback rate.
  animation->set_direction(Animation::Direction::NORMAL);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // ALTERNATE direction with negative playback rate.
  animation->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // REVERSE direction with negative playback rate.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // ALTERNATE reverse direction with negative playback rate.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      player_impl_->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);
}

TEST_F(ElementAnimationsTest, NewlyPushedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(player_->needs_to_start_animations());
  int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.5f, 1.f, false);
  EXPECT_TRUE(player_->needs_to_start_animations());

  EXPECT_FALSE(player_impl_->needs_to_start_animations());
  PushProperties();
  EXPECT_TRUE(player_impl_->needs_to_start_animations());

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_pending_elements());
  EXPECT_FALSE(
      player_impl_->GetAnimationById(animation_id)->affects_active_elements());

  element_animations_impl_->Animate(kInitialTickTime);
  EXPECT_FALSE(player_impl_->needs_to_start_animations());
  element_animations_impl_->UpdateState(true, events.get());

  // Since the animation hasn't been activated, it should still be STARTING
  // rather than RUNNING.
  EXPECT_EQ(Animation::STARTING,
            player_impl_->GetAnimationById(animation_id)->run_state());

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_pending_elements());
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_active_elements());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state and the active observer should start to get ticked.
  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimationById(animation_id)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ActivationBetweenAnimateAndUpdateState) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.5f, 1.f, true);

  PushProperties();

  EXPECT_TRUE(player_impl_->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetAnimationById(animation_id)->run_state());
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_pending_elements());
  EXPECT_FALSE(
      player_impl_->GetAnimationById(animation_id)->affects_active_elements());

  element_animations_impl_->Animate(kInitialTickTime);

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateAnimations();
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_pending_elements());
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_active_elements());

  element_animations_impl_->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state.
  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimationById(animation_id)->run_state());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(500));

  // Both elements should have been ticked.
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenTransformAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedTransformToPlayer(player_.get(), 1.0, 1, 1);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();

  // animations_impl hasn't yet ticked at/past the end of the animation.
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int animation_id = AddAnimatedTransformToPlayer(player_.get(), 10.0, 2, 2);
  int animation2_id = AddAnimatedTransformToPlayer(player_.get(), 10.0, 2, 1);
  player_->GetAnimationById(animation2_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->GetAnimationById(animation2_id)
      ->set_fill_mode(Animation::FillMode::NONE);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  // animation1 is in effect currently and animation2 isn't. As the element has
  // atleast one animation that's in effect currently, client should be notified
  // that the transform is currently animating.
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  player_->RemoveAnimation(animation_id);
  player_->RemoveAnimation(animation2_id);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  animation_id = AddAnimatedTransformToPlayer(player_.get(), 10.0, 3, 3);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  player_impl_->AbortAnimations(TargetProperty::TRANSFORM, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(4000));
  element_animations_impl_->UpdateState(true, events.get());

  element_animations_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  animation_id = AddAnimatedTransformToPlayer(player_.get(), 1.0, 1, 6);
  player_->GetAnimationById(animation_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->GetAnimationById(animation_id)
      ->set_fill_mode(Animation::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenOpacityAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 1.f,
                               false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  PushProperties();

  // animations_impl hasn't yet ticked at/past the end of the animation.
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int animation_id = AddOpacityTransitionToPlayer(
      player_.get(), 10.0, 0.f, 1.f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  player_->RemoveAnimation(animation_id);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  animation_id = AddOpacityTransitionToPlayer(player_.get(), 10.0, 0.f, 0.5f,
                                              false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  player_impl_->AbortAnimations(TargetProperty::OPACITY, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(4000));
  element_animations_impl_->UpdateState(true, events.get());

  element_animations_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  animation_id = AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 0.5f,
                                              false /*use_timing_function*/);
  player_->GetAnimationById(animation_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->GetAnimationById(animation_id)
      ->set_fill_mode(Animation::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenFilterAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedFilterToPlayer(player_.get(), 1.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();

  // animations_impl hasn't yet ticked at/past the end of the animation.
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int animation_id = AddAnimatedFilterToPlayer(player_.get(), 10.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  player_->RemoveAnimation(animation_id);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  animation_id = AddAnimatedFilterToPlayer(player_.get(), 10.0, 0.f, 0.5f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(2000));
  element_animations_impl_->UpdateState(true, events.get());

  player_->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  player_impl_->AbortAnimations(TargetProperty::FILTER, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(4000));
  element_animations_impl_->UpdateState(true, events.get());

  element_animations_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  animation_id = AddAnimatedFilterToPlayer(player_.get(), 1.0, 0.f, 0.5f);
  player_->GetAnimationById(animation_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->GetAnimationById(animation_id)
      ->set_fill_mode(Animation::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  element_animations_impl_->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  AddOpacityTransitionToPlayer(player_.get(), 1, 1.f, 2.f, true);

  element_animations_->Animate(kInitialTickTime);
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedNegativeOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  AddOpacityTransitionToPlayer(player_.get(), 1, 0.f, -2.f, true);

  element_animations_->Animate(kInitialTickTime);
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushedDeletedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.5f, 1.f, true);

  PushProperties();
  player_impl_->ActivateAnimations();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());
  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimationById(animation_id)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_pending_elements());
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_active_elements());

  // Delete the animation on the main-thread animations.
  player_->RemoveAnimation(
      player_->GetAnimation(TargetProperty::OPACITY)->id());
  PushProperties();

  // The animation should no longer affect pending elements.
  EXPECT_FALSE(
      player_impl_->GetAnimationById(animation_id)->affects_pending_elements());
  EXPECT_TRUE(
      player_impl_->GetAnimationById(animation_id)->affects_active_elements());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(500));
  element_animations_impl_->UpdateState(true, events.get());

  // Only the active observer should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateAnimations();

  // Activation should cause the animation to be deleted.
  EXPECT_FALSE(player_impl_->has_any_animation());
}

// Tests that an animation that affects only active elements won't block
// an animation that affects only pending elements from starting.
TEST_F(ElementAnimationsTest, StartAnimationsAffectingDifferentObservers) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int first_animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.f, 1.f, true);

  PushProperties();
  player_impl_->ActivateAnimations();
  element_animations_impl_->Animate(kInitialTickTime);
  element_animations_impl_->UpdateState(true, events.get());

  // Remove the first animation from the main-thread animations, and add a
  // new animation affecting the same property.
  player_->RemoveAnimation(
      player_->GetAnimation(TargetProperty::OPACITY)->id());
  const int second_animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 1.f, 0.5f, true);
  PushProperties();

  // The original animation should only affect active elements, and the new
  // animation should only affect pending elements.
  EXPECT_FALSE(player_impl_->GetAnimationById(first_animation_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->GetAnimationById(first_animation_id)
                  ->affects_active_elements());
  EXPECT_TRUE(player_impl_->GetAnimationById(second_animation_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(player_impl_->GetAnimationById(second_animation_id)
                   ->affects_active_elements());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(500));
  element_animations_impl_->UpdateState(true, events.get());

  // The original animation should still be running, and the new animation
  // should be starting.
  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimationById(first_animation_id)->run_state());
  EXPECT_EQ(Animation::STARTING,
            player_impl_->GetAnimationById(second_animation_id)->run_state());

  // The active observer should have been ticked by the original animation,
  // and the pending observer should have been ticked by the new animation.
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateAnimations();

  // The original animation should have been deleted, and the new animation
  // should now affect both elements.
  EXPECT_FALSE(player_impl_->GetAnimationById(first_animation_id));
  EXPECT_TRUE(player_impl_->GetAnimationById(second_animation_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->GetAnimationById(second_animation_id)
                  ->affects_active_elements());

  element_animations_impl_->Animate(kInitialTickTime +
                                    TimeDelta::FromMilliseconds(1000));
  element_animations_impl_->UpdateState(true, events.get());

  // The new animation should be running, and the active observer should have
  // been ticked at the new animation's starting point.
  EXPECT_EQ(Animation::RUNNING,
            player_impl_->GetAnimationById(second_animation_id)->run_state());
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(1.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsCurrentlyAnimatingProperty) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Create an animation that initially affects only pending elements.
  std::unique_ptr<Animation> animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animation->set_affects_active_elements(false);

  player_->AddAnimation(std::move(animation));
  element_animations_->Animate(kInitialTickTime);
  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::ACTIVE));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->HasActiveAnimation());

  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::ACTIVE));

  player_->ActivateAnimations();

  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::PENDING));
  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::ACTIVE));

  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(10));
  element_animations_->UpdateState(true, nullptr);

  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::PENDING));
  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::ACTIVE));

  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Tick past the end of the animation.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(1100));
  element_animations_->UpdateState(true, nullptr);

  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::FILTER,
                                                     ElementListType::ACTIVE));

  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsAnimatingPropertyTimeOffsetFillMode) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Create an animation that initially affects only pending elements, and has
  // a start delay of 2 seconds.
  std::unique_ptr<Animation> animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(TimeDelta::FromMilliseconds(-2000));
  animation->set_affects_active_elements(false);

  player_->AddAnimation(std::move(animation));

  element_animations_->Animate(kInitialTickTime);

  // Since the animation has a start delay, the elements it affects have a
  // potentially running transform animation but aren't currently animating
  // transform.
  EXPECT_TRUE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  player_->ActivateAnimations();

  EXPECT_TRUE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->IsPotentiallyAnimatingProperty(TargetProperty::OPACITY,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(player_->HasActiveAnimation());
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  element_animations_->UpdateState(true, nullptr);

  // Tick past the start delay.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(2000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->IsPotentiallyAnimatingProperty(TargetProperty::OPACITY,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::PENDING));
  EXPECT_TRUE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                    ElementListType::ACTIVE));

  // After the animaton finishes, the elements it affects have neither a
  // potentially running transform animation nor a currently running transform
  // animation.
  element_animations_->Animate(kInitialTickTime +
                               TimeDelta::FromMilliseconds(4000));
  element_animations_->UpdateState(true, nullptr);
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::PENDING));
  EXPECT_FALSE(player_->IsCurrentlyAnimatingProperty(TargetProperty::OPACITY,
                                                     ElementListType::ACTIVE));
}

}  // namespace
}  // namespace cc
