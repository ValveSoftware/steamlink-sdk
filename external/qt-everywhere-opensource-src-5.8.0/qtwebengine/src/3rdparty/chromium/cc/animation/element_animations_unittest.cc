// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "cc/animation/animation_delegate.h"
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
};

// See animation_player_unittest.cc for integration with AnimationPlayer.

TEST_F(ElementAnimationsTest, AttachToLayerInActiveTree) {
  // Set up the layer which is in active tree for main thread and not
  // yet passed onto the impl thread.
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);

  EXPECT_TRUE(client_.IsElementInList(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.IsElementInList(element_id_, ElementListType::PENDING));

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachPlayer(player_);
  player_->AttachElement(element_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->element_animations();
  EXPECT_TRUE(element_animations);

  EXPECT_TRUE(element_animations->has_element_in_active_list());
  EXPECT_FALSE(element_animations->has_element_in_pending_list());

  host_->PushPropertiesTo(host_impl_);

  GetImplTimelineAndPlayerByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  EXPECT_FALSE(element_animations_impl->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl->has_element_in_pending_list());

  // Create the layer in the impl active tree.
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl->has_element_in_pending_list());

  EXPECT_TRUE(
      client_impl_.IsElementInList(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(
      client_impl_.IsElementInList(element_id_, ElementListType::PENDING));

  // kill layer on main thread.
  client_.UnregisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations, player_->element_animations());
  EXPECT_FALSE(element_animations->has_element_in_active_list());
  EXPECT_FALSE(element_animations->has_element_in_pending_list());

  // Sync doesn't detach LayerImpl.
  host_->PushPropertiesTo(host_impl_);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_TRUE(element_animations_impl->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl->has_element_in_pending_list());

  // Kill layer on impl thread in pending tree.
  client_impl_.UnregisterElement(element_id_, ElementListType::PENDING);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_TRUE(element_animations_impl->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl->has_element_in_pending_list());

  // Kill layer on impl thread in active tree.
  client_impl_.UnregisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_FALSE(element_animations_impl->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl->has_element_in_pending_list());

  // Sync doesn't change anything.
  host_->PushPropertiesTo(host_impl_);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_FALSE(element_animations_impl->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl->has_element_in_pending_list());

  player_->DetachElement();
  EXPECT_FALSE(player_->element_animations());

  // Release ptrs now to test the order of destruction.
  ReleaseRefPtrs();
}

TEST_F(ElementAnimationsTest, AttachToNotYetCreatedLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);

  host_->PushPropertiesTo(host_impl_);

  GetImplTimelineAndPlayerByID();

  player_->AttachElement(element_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->element_animations();
  EXPECT_TRUE(element_animations);

  EXPECT_FALSE(element_animations->has_element_in_active_list());
  EXPECT_FALSE(element_animations->has_element_in_pending_list());

  host_->PushPropertiesTo(host_impl_);

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  EXPECT_FALSE(element_animations_impl->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl->has_element_in_pending_list());

  // Create layer.
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations->has_element_in_active_list());
  EXPECT_FALSE(element_animations->has_element_in_pending_list());

  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  EXPECT_FALSE(element_animations_impl->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl->has_element_in_pending_list());

  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl->has_element_in_pending_list());
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

  host_->PushPropertiesTo(host_impl_);
  GetImplTimelineAndPlayerByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  int list_size_before = 0;
  for (const ElementAnimations::PlayersListNode* node =
           element_animations_impl->players_list().head();
       node != element_animations_impl->players_list().end();
       node = node->next()) {
    const AnimationPlayer* player_impl = node->value();
    EXPECT_TRUE(timeline_->GetPlayerById(player_impl->id()));
    ++list_size_before;
  }
  EXPECT_EQ(3, list_size_before);

  player2->DetachElement();
  EXPECT_FALSE(player2->element_animations());
  EXPECT_EQ(element_animations, player_->element_animations());
  EXPECT_EQ(element_animations, player1->element_animations());

  host_->PushPropertiesTo(host_impl_);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());

  int list_size_after = 0;
  for (const ElementAnimations::PlayersListNode* node =
           element_animations_impl->players_list().head();
       node != element_animations_impl->players_list().end();
       node = node->next()) {
    const AnimationPlayer* player_impl = node->value();
    EXPECT_TRUE(timeline_->GetPlayerById(player_impl->id()));
    ++list_size_after;
  }
  EXPECT_EQ(2, list_size_after);
}

TEST_F(ElementAnimationsTest, SyncNewAnimation) {
  auto animations_impl = ElementAnimations::Create();
  animations_impl->set_has_element_in_active_list(true);

  auto animations = ElementAnimations::Create();
  animations->set_has_element_in_active_list(true);

  EXPECT_FALSE(animations_impl->GetAnimation(TargetProperty::OPACITY));

  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());
  EXPECT_FALSE(animations_impl->needs_to_start_animations_for_testing());

  int animation_id =
      AddOpacityTransitionToElementAnimations(animations.get(), 1, 0, 1, false);
  EXPECT_TRUE(animations->needs_to_start_animations_for_testing());

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(animations_impl->needs_to_start_animations_for_testing());
  animations_impl->ActivateAnimations();

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());
}

TEST_F(ElementAnimationsTest,
       SyncScrollOffsetAnimationRespectsHasSetInitialValue) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  EXPECT_FALSE(animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET));

  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());
  EXPECT_FALSE(animations_impl->needs_to_start_animations_for_testing());

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
  animations->AddAnimation(std::move(animation_fixed));
  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_VECTOR2DF_EQ(initial_value,
                      animations_impl->GetAnimationById(animation1_id)
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
  animations->AddAnimation(std::move(animation));
  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_VECTOR2DF_EQ(provider_initial_value,
                      animations_impl->GetAnimationById(animation2_id)
                          ->curve()
                          ->ToScrollOffsetAnimationCurve()
                          ->GetValue(base::TimeDelta()));
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST_F(ElementAnimationsTest, DoNotClobberStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  EXPECT_FALSE(animations_impl->GetAnimation(TargetProperty::OPACITY));

  int animation_id =
      AddOpacityTransitionToElementAnimations(animations.get(), 1, 0, 1, false);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());

  auto events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  animations->NotifyAnimationStarted(events->events_[0]);
  EXPECT_EQ(animations->GetAnimationById(animation_id)->start_time(),
            animations_impl->GetAnimationById(animation_id)->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, nullptr);
  EXPECT_EQ(animations->GetAnimationById(animation_id)->start_time(),
            animations_impl->GetAnimationById(animation_id)->start_time());
}

TEST_F(ElementAnimationsTest, UseSpecifiedStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  int animation_id =
      AddOpacityTransitionToElementAnimations(animations.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  animations->GetAnimation(TargetProperty::OPACITY)->set_start_time(start_time);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());

  auto events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  animations->NotifyAnimationStarted(events->events_[0]);

  EXPECT_EQ(start_time,
            animations->GetAnimationById(animation_id)->start_time());
  EXPECT_EQ(animations->GetAnimationById(animation_id)->start_time(),
            animations_impl->GetAnimationById(animation_id)->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, nullptr);
  EXPECT_EQ(start_time,
            animations->GetAnimationById(animation_id)->start_time());
  EXPECT_EQ(animations->GetAnimationById(animation_id)->start_time(),
            animations_impl->GetAnimationById(animation_id)->start_time());
}

// Tests that animationss activate and deactivate as expected.
TEST_F(ElementAnimationsTest, Activation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  AnimationHost* host = client_.host();
  AnimationHost* host_impl = client_impl_.host();

  auto events = host_impl_->CreateEvents();

  EXPECT_EQ(1u, host->all_element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->all_element_animations_for_testing().size());

  // Initially, both animationss should be inactive.
  EXPECT_EQ(0u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl->active_element_animations_for_testing().size());

  AddOpacityTransitionToElementAnimations(animations.get(), 1, 0, 1, false);
  // The main thread animations should now be active.
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  // Both animationss should now be active.
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->active_element_animations_for_testing().size());

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  animations->NotifyAnimationStarted(events->events_[0]);

  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->active_element_animations_for_testing().size());

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, nullptr);
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, nullptr);
  EXPECT_EQ(Animation::FINISHED,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(1u, host->active_element_animations_for_testing().size());

  events = host_impl_->CreateEvents();

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1500));
  animations_impl->UpdateState(true, events.get());

  EXPECT_EQ(
      Animation::WAITING_FOR_DELETION,
      animations_impl->GetAnimation(TargetProperty::OPACITY)->run_state());
  // The impl thread animations should have de-activated.
  EXPECT_EQ(0u, host_impl->active_element_animations_for_testing().size());

  EXPECT_EQ(1u, events->events_.size());
  animations->NotifyAnimationFinished(events->events_[0]);
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  animations->UpdateState(true, nullptr);

  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  // The main thread animations should have de-activated.
  EXPECT_EQ(0u, host->active_element_animations_for_testing().size());

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->has_any_animation());
  EXPECT_FALSE(animations_impl->has_any_animation());
  EXPECT_EQ(0u, host->active_element_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl->active_element_animations_for_testing().size());
}

TEST_F(ElementAnimationsTest, SyncPause) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  EXPECT_FALSE(animations_impl->GetAnimation(TargetProperty::OPACITY));

  // Two steps, three ranges: [0-1) -> 0.2, [1-2) -> 0.3, [2-3] -> 0.4.
  const double duration = 3.0;
  const int animation_id = AddOpacityStepsToElementAnimations(
      animations.get(), duration, 0.2f, 0.4f, 2);

  // Set start offset to be at the beginning of the second range.
  animations->GetAnimationById(animation_id)
      ->set_time_offset(TimeDelta::FromSecondsD(1.01));

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());

  TimeTicks time = kInitialTickTime;

  // Start the animations on each animations.
  auto events = host_impl_->CreateEvents();
  animations_impl->Animate(time);
  animations_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());

  animations->Animate(time);
  animations->UpdateState(true, nullptr);
  animations->NotifyAnimationStarted(events->events_[0]);

  EXPECT_EQ(Animation::RUNNING,
            animations_impl->GetAnimationById(animation_id)->run_state());
  EXPECT_EQ(Animation::RUNNING,
            animations->GetAnimationById(animation_id)->run_state());

  EXPECT_EQ(0.3f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.3f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_EQ(kInitialTickTime,
            animations->GetAnimationById(animation_id)->start_time());
  EXPECT_EQ(kInitialTickTime,
            animations_impl->GetAnimationById(animation_id)->start_time());

  // Pause the animation at the middle of the second range so the offset
  // delays animation until the middle of the third range.
  animations->PauseAnimation(animation_id, TimeDelta::FromSecondsD(1.5));
  EXPECT_EQ(Animation::PAUSED,
            animations->GetAnimationById(animation_id)->run_state());

  // The pause run state change should make it to the impl thread animations.
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  // Advance time so it stays within the first range.
  time += TimeDelta::FromMilliseconds(10);
  animations->Animate(time);
  animations_impl->Animate(time);

  EXPECT_EQ(Animation::PAUSED,
            animations_impl->GetAnimationById(animation_id)->run_state());

  // Opacity value doesn't depend on time if paused at specified time offset.
  EXPECT_EQ(0.4f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.4f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, DoNotSyncFinishedAnimation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  EXPECT_FALSE(animations_impl->GetAnimation(TargetProperty::OPACITY));

  int animation_id =
      AddOpacityTransitionToElementAnimations(animations.get(), 1, 0, 1, false);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());

  events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // Notify main thread animations that the animation has started.
  animations->NotifyAnimationStarted(events->events_[0]);

  // Complete animation on impl thread.
  events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime + TimeDelta::FromSeconds(1));
  animations_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  animations->NotifyAnimationFinished(events->events_[0]);

  animations->Animate(kInitialTickTime + TimeDelta::FromSeconds(2));
  animations->UpdateState(true, nullptr);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->GetAnimationById(animation_id));
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id));
}

// Ensure that a finished animation is eventually deleted by both the
// main-thread and the impl-thread animationss.
TEST_F(ElementAnimationsTest, AnimationsAreDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  AddOpacityTransitionToElementAnimations(animations.get(), 1.0, 0.0f, 1.0f,
                                          false);
  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, nullptr);
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  animations_impl->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations_impl->UpdateState(true, events.get());

  // There should be a STARTED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  animations->NotifyAnimationStarted(events->events_[0]);

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, nullptr);

  EXPECT_FALSE(host_->animation_waiting_for_deletion());
  EXPECT_FALSE(host_impl_->animation_waiting_for_deletion());

  events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(2000));
  animations_impl->UpdateState(true, events.get());

  EXPECT_TRUE(host_impl_->animation_waiting_for_deletion());

  // There should be a FINISHED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  // Neither animations should have deleted the animation yet.
  EXPECT_TRUE(animations->GetAnimation(TargetProperty::OPACITY));
  EXPECT_TRUE(animations_impl->GetAnimation(TargetProperty::OPACITY));

  animations->NotifyAnimationFinished(events->events_[0]);

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->animation_waiting_for_deletion());

  animations->PushPropertiesTo(animations_impl.get());

  // Both animationss should now have deleted the animation. The impl animations
  // should have deleted the animation even though activation has not occurred,
  // since the animation was already waiting for deletion when
  // PushPropertiesTo was called.
  EXPECT_FALSE(animations->has_any_animation());
  EXPECT_FALSE(animations_impl->has_any_animation());
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

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());
  animations->AddAnimation(std::move(to_add));
  EXPECT_TRUE(animations->needs_to_start_animations_for_testing());
  animations->Animate(kInitialTickTime);
  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

TEST_F(ElementAnimationsTest, FilterTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

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
  animations->AddAnimation(std::move(animation));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(start_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(1u,
            client_.GetFilters(element_id_, ElementListType::ACTIVE).size());
  EXPECT_EQ(FilterOperation::CreateBrightnessFilter(1.5f),
            client_.GetFilters(element_id_, ElementListType::ACTIVE).at(0));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(end_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_needs_synchronized_start_time(true);
  animations->AddAnimation(std::move(animation));

  client_impl_.SetScrollOffsetForAnimation(initial_value);
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration =
      animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET)
          ->curve()
          ->Duration();
  EXPECT_EQ(duration, animations->GetAnimation(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_TRUE(animations_impl->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  // Scroll offset animations should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations->NotifyAnimationStarted(events->events_[0]);
  animations->Animate(kInitialTickTime + duration / 2);
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime + duration / 2);
  animations_impl->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations_impl->Animate(kInitialTickTime + duration);
  animations_impl->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations_impl->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations->Animate(kInitialTickTime + duration);
  animations->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransitionOnImplOnly) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

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
  animations_impl->AddAnimation(std::move(animation));

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_TRUE(animations_impl->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  // Scroll offset animations should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  TimeDelta duration = TimeDelta::FromMicroseconds(
      duration_in_seconds * base::Time::kMicrosecondsPerSecond);

  animations_impl->Animate(kInitialTickTime + duration / 2);
  animations_impl->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations_impl->Animate(kInitialTickTime + duration);
  animations_impl->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations_impl->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

// Ensure that when the impl animations doesn't have a value provider,
// the main-thread animations's value provider is used to obtain the intial
// scroll offset.
TEST_F(ElementAnimationsTest, ScrollOffsetTransitionNoImplProvider) {
  CreateTestLayer(false, false);
  CreateTestImplLayer(ElementListType::PENDING);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  EXPECT_TRUE(animations_impl->has_element_in_pending_list());
  EXPECT_FALSE(animations_impl->has_element_in_active_list());

  auto events = host_impl_->CreateEvents();

  gfx::ScrollOffset initial_value(500.f, 100.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  std::unique_ptr<Animation> animation(
      Animation::Create(std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  animation->set_needs_synchronized_start_time(true);
  animations->AddAnimation(std::move(animation));

  client_.SetScrollOffsetForAnimation(initial_value);
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration =
      animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET)
          ->curve()
          ->Duration();
  EXPECT_EQ(duration, animations->GetAnimation(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, nullptr);

  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(gfx::ScrollOffset(), client_impl_.GetScrollOffset(
                                     element_id_, ElementListType::PENDING));

  animations_impl->Animate(kInitialTickTime);

  EXPECT_TRUE(animations_impl->HasActiveAnimation());
  EXPECT_EQ(initial_value, client_impl_.GetScrollOffset(
                               element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  animations_impl->UpdateState(true, events.get());
  DCHECK_EQ(1UL, events->events_.size());

  // Scroll offset animations should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations->NotifyAnimationStarted(events->events_[0]);
  animations->Animate(kInitialTickTime + duration / 2);
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime + duration / 2);
  animations_impl->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::PENDING));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations_impl->Animate(kInitialTickTime + duration);
  animations_impl->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::PENDING));
  EXPECT_FALSE(animations_impl->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  animations->Animate(kInitialTickTime + duration);
  animations->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

TEST_F(ElementAnimationsTest, ScrollOffsetRemovalClearsScrollDelta) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

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
  animations->AddAnimation(std::move(animation));
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  animations->RemoveAnimation(animation_id);
  EXPECT_TRUE(animations->scroll_offset_animation_was_interrupted());

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(animations_impl->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  // Now, test the 2-argument version of RemoveAnimation.
  curve = ScrollOffsetAnimationCurve::Create(
      target_value, CubicBezierTimingFunction::CreatePreset(
                        CubicBezierTimingFunction::EaseType::EASE_IN_OUT));
  animation = Animation::Create(std::move(curve), animation_id, 0,
                                TargetProperty::SCROLL_OFFSET);
  animation->set_needs_synchronized_start_time(true);
  animations->AddAnimation(std::move(animation));
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  animations->RemoveAnimation(animation_id);
  EXPECT_TRUE(animations->scroll_offset_animation_was_interrupted());

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(animations_impl->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  // Check that removing non-scroll-offset animations does not cause
  // scroll_offset_animation_was_interrupted() to get set.
  animation_id =
      AddAnimatedTransformToElementAnimations(animations.get(), 1.0, 1, 2);
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  animations->RemoveAnimation(animation_id);
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  animation_id =
      AddAnimatedFilterToElementAnimations(animations.get(), 1.0, 0.1f, 0.2f);
  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());

  animations->RemoveAnimation(animation_id);
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animations->scroll_offset_animation_was_interrupted());

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations_impl->scroll_offset_animation_was_interrupted());
}

// Tests that impl-only animations lead to start and finished notifications
// on the impl thread animations's animation delegate.
TEST_F(ElementAnimationsTest,
       NotificationsForImplOnlyAnimationsAreSentToImplThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

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
  animations_impl->AddAnimation(std::move(to_add));

  EXPECT_FALSE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime + duration);
  EXPECT_EQ(duration,
            animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET)
                ->curve()
                ->Duration());
  animations_impl->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_TRUE(delegate.finished());
}

// Tests that specified start times are sent to the main thread delegate
TEST_F(ElementAnimationsTest, SpecifiedStartTimesAreSentToMainThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  int animation_id =
      AddOpacityTransitionToElementAnimations(animations.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  animations->GetAnimation(TargetProperty::OPACITY)->set_start_time(start_time);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());

  auto events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  animations->NotifyAnimationStarted(events->events_[0]);

  // Validate start time on the main thread delegate.
  EXPECT_EQ(start_time, delegate.start_time());
}

// Tests animations that are waiting for a synchronized start time do not
// finish.
TEST_F(ElementAnimationsTest,
       AnimationsWaitingForStartTimeDoNotFinishIfTheyOutwaitTheirFinish) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);

  // We should pause at the first keyframe indefinitely waiting for that
  // animation to start.
  animations->AddAnimation(std::move(to_add));
  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Send the synchronized start time.
  animations->NotifyAnimationStarted(AnimationEvent(
      AnimationEvent::STARTED, ElementId(), 1, TargetProperty::OPACITY,
      kInitialTickTime + TimeDelta::FromMilliseconds(2000)));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(5000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST_F(ElementAnimationsTest, TrivialQueuing) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());

  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));

  EXPECT_TRUE(animations->needs_to_start_animations_for_testing());

  animations->Animate(kInitialTickTime);

  // The second animation still needs to be started.
  EXPECT_TRUE(animations->needs_to_start_animations_for_testing());

  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(animations->needs_to_start_animations_for_testing());
  animations->UpdateState(true, events.get());
  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());

  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST_F(ElementAnimationsTest, Interrupt) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  animations->AbortAnimations(TargetProperty::OPACITY);
  animations->AddAnimation(std::move(to_add));

  // Since the previous animation was aborted, the new animation should start
  // right in this call to animate.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is
// free.
TEST_F(ElementAnimationsTest, ScheduleTogetherWhenAPropertyIsBlocked) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 2,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(animations->HasActiveAnimation());
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The float animation should have started at time 1 and should be done.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and
// another animation queued to start when the shorter animation finishes (should
// wait for both to finish).
TEST_F(ElementAnimationsTest, ScheduleTogetherWithAnAnimWaiting) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2)), 1,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));

  // Animations with id 1 should both start now.
  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The opacity animation should have finished at time 1, but the group
  // of animations with id 1 don't finish until time 2 because of the length
  // of the transform animation.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The second opacity animation should start at time 2 and should be done by
  // time 3.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST_F(ElementAnimationsTest, TrivialLooping) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(3);
  animations->AddAnimation(std::move(to_add));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1250));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1750));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2250));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2750));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animations->UpdateState(true, events.get());
  EXPECT_FALSE(animations->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Just be extra sure.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST_F(ElementAnimationsTest, InfiniteLooping) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(-1);
  animations->AddAnimation(std::move(to_add));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1250));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1750));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations->Animate(kInitialTickTime +
                      TimeDelta::FromMilliseconds(1073741824250));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime +
                      TimeDelta::FromMilliseconds(1073741824750));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animations->GetAnimation(TargetProperty::OPACITY));
  animations->GetAnimation(TargetProperty::OPACITY)
      ->SetRunState(Animation::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(750));
  EXPECT_FALSE(animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that pausing and resuming work as expected.
TEST_F(ElementAnimationsTest, PauseResume) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animations->GetAnimation(TargetProperty::OPACITY));
  animations->GetAnimation(TargetProperty::OPACITY)
      ->SetRunState(Animation::PAUSED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(500));

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animations->GetAnimation(TargetProperty::OPACITY));
  animations->GetAnimation(TargetProperty::OPACITY)
      ->SetRunState(Animation::RUNNING,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1024250));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1024500));
  animations->UpdateState(true, events.get());
  EXPECT_FALSE(animations->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AbortAGroupedAnimation) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  const int animation_id = 2;
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1, 1,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      animation_id, 1, TargetProperty::OPACITY));
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.75f)),
      3, 2, TargetProperty::OPACITY));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animations->GetAnimationById(animation_id));
  animations->GetAnimationById(animation_id)
      ->SetRunState(Animation::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(!animations->HasActiveAnimation());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushUpdatesWhenSynchronizedStartTimeNeeded) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  std::unique_ptr<Animation> to_add(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      0, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);
  animations->AddAnimation(std::move(to_add));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_TRUE(animations->HasActiveAnimation());
  Animation* active_animation =
      animations->GetAnimation(TargetProperty::OPACITY);
  EXPECT_TRUE(active_animation);
  EXPECT_TRUE(active_animation->needs_synchronized_start_time());

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();

  active_animation = animations_impl->GetAnimation(TargetProperty::OPACITY);
  EXPECT_TRUE(active_animation);
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            active_animation->run_state());
}

// Tests that skipping a call to UpdateState works as expected.
TEST_F(ElementAnimationsTest, SkipUpdateState) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  auto events = host_impl_->CreateEvents();

  std::unique_ptr<Animation> first_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  first_animation->set_is_controlling_instance_for_test(true);
  animations->AddAnimation(std::move(first_animation));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());

  std::unique_ptr<Animation> second_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));
  second_animation->set_is_controlling_instance_for_test(true);
  animations->AddAnimation(std::move(second_animation));

  // Animate but don't UpdateState.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  events = host_impl_->CreateEvents();
  animations->UpdateState(true, events.get());

  // Should have one STARTED event and one FINISHED event.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_NE(events->events_[0].type, events->events_[1].type);

  // The float transition should still be at its starting point.
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animations->UpdateState(true, events.get());

  // The float tranisition should now be done.
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->HasActiveAnimation());
}

// Tests that an animation animations with only a pending observer gets ticked
// but doesn't progress animations past the STARTING state.
TEST_F(ElementAnimationsTest, InactiveObserverGetsTicked) {
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  const int id = 1;
  animations->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.5f, 1.f)),
      id, TargetProperty::OPACITY));

  // Without an observer, the animation shouldn't progress to the STARTING
  // state.
  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());

  CreateTestImplLayer(ElementListType::PENDING);

  // With only a pending observer, the animation should progress to the
  // STARTING state and get ticked at its starting point, but should not
  // progress to RUNNING.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::STARTING,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  // Even when already in the STARTING state, the animation should stay
  // there, and shouldn't be ticked past its starting point.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::STARTING,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  // Now that an active observer has been added, the animation should still
  // initially tick at its starting point, but should now progress to RUNNING.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animations->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(Animation::RUNNING,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The animation should now tick past its starting point.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(3500));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TransformAnimationBounds) {
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

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
  animations_impl->AddAnimation(std::move(animation));

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
  animations_impl->AddAnimation(std::move(animation));

  gfx::BoxF box(1.f, 2.f, -1.f, 3.f, 4.f, 5.f);
  gfx::BoxF bounds;

  EXPECT_TRUE(animations_impl->TransformAnimationBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -4.f, 13.f, 19.f, 20.f).ToString(),
            bounds.ToString());

  animations_impl->GetAnimationById(1)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));

  // Only the unfinished animation should affect the animated bounds.
  EXPECT_TRUE(animations_impl->TransformAnimationBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -4.f, 7.f, 16.f, 20.f).ToString(),
            bounds.ToString());

  animations_impl->GetAnimationById(2)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));

  // There are no longer any running animations.
  EXPECT_FALSE(animations_impl->HasTransformAnimationThatInflatesBounds());

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
  animations_impl->AddAnimation(std::move(animation));
  EXPECT_FALSE(animations_impl->TransformAnimationBoundsForBox(box, &bounds));
}

// Tests that AbortAnimations aborts all animations targeting the specified
// property.
TEST_F(ElementAnimationsTest, AbortAnimations) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  // Start with several animations, and allow some of them to reach the finished
  // state.
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1, 1,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, 2, TargetProperty::OPACITY));
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 3, 3,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 4, 4,
      TargetProperty::TRANSFORM));
  animations->AddAnimation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      5, 5, TargetProperty::OPACITY));

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, nullptr);
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, nullptr);

  EXPECT_EQ(Animation::FINISHED, animations->GetAnimationById(1)->run_state());
  EXPECT_EQ(Animation::FINISHED, animations->GetAnimationById(2)->run_state());
  EXPECT_EQ(Animation::RUNNING, animations->GetAnimationById(3)->run_state());
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations->GetAnimationById(4)->run_state());
  EXPECT_EQ(Animation::RUNNING, animations->GetAnimationById(5)->run_state());

  animations->AbortAnimations(TargetProperty::TRANSFORM);

  // Only un-finished TRANSFORM animations should have been aborted.
  EXPECT_EQ(Animation::FINISHED, animations->GetAnimationById(1)->run_state());
  EXPECT_EQ(Animation::FINISHED, animations->GetAnimationById(2)->run_state());
  EXPECT_EQ(Animation::ABORTED, animations->GetAnimationById(3)->run_state());
  EXPECT_EQ(Animation::ABORTED, animations->GetAnimationById(4)->run_state());
  EXPECT_EQ(Animation::RUNNING, animations->GetAnimationById(5)->run_state());
}

// An animation aborted on the main thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, MainThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  int animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1.0, 0.f, 1.f, false);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));

  animations->AbortAnimations(TargetProperty::OPACITY);
  EXPECT_EQ(Animation::ABORTED,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_FALSE(host_->animation_waiting_for_deletion());
  EXPECT_FALSE(host_impl_->animation_waiting_for_deletion());

  animations->Animate(kInitialTickTime);
  animations->UpdateState(true, nullptr);
  EXPECT_FALSE(host_->animation_waiting_for_deletion());
  EXPECT_EQ(Animation::ABORTED,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_FALSE(animations->GetAnimationById(animation_id));
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id));
}

// An animation aborted on the impl thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, ImplThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  int animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1.0, 0.f, 1.f, false);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));

  animations_impl->AbortAnimations(TargetProperty::OPACITY);
  EXPECT_EQ(
      Animation::ABORTED,
      animations_impl->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_FALSE(host_->animation_waiting_for_deletion());
  EXPECT_FALSE(host_impl_->animation_waiting_for_deletion());

  auto events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_TRUE(host_impl_->animation_waiting_for_deletion());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[0].type);
  EXPECT_EQ(
      Animation::WAITING_FOR_DELETION,
      animations_impl->GetAnimation(TargetProperty::OPACITY)->run_state());

  animations->NotifyAnimationAborted(events->events_[0]);
  EXPECT_EQ(Animation::ABORTED,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(delegate.aborted());

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->animation_waiting_for_deletion());
  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            animations->GetAnimation(TargetProperty::OPACITY)->run_state());

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->GetAnimationById(animation_id));
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id));
}

// Test that an impl-only scroll offset animation that needs to be completed on
// the main thread gets deleted.
TEST_F(ElementAnimationsTest, ImplThreadTakeoverAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

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
  animations_impl->AddAnimation(std::move(animation));

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));

  const bool needs_completion = true;
  animations_impl->AbortAnimations(TargetProperty::SCROLL_OFFSET,
                                   needs_completion);
  EXPECT_EQ(Animation::ABORTED_BUT_NEEDS_COMPLETION,
            animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET)
                ->run_state());
  EXPECT_FALSE(host_->animation_waiting_for_deletion());
  EXPECT_FALSE(host_impl_->animation_waiting_for_deletion());

  auto events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_TRUE(delegate_impl.finished());
  EXPECT_TRUE(host_impl_->animation_waiting_for_deletion());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::TAKEOVER, events->events_[0].type);
  EXPECT_EQ(123, events->events_[0].animation_start_time);
  EXPECT_EQ(
      target_value,
      events->events_[0].curve->ToScrollOffsetAnimationCurve()->target_value());
  EXPECT_EQ(Animation::WAITING_FOR_DELETION,
            animations_impl->GetAnimation(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  animations->NotifyAnimationTakeover(events->events_[0]);
  EXPECT_TRUE(delegate.takeover());

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations->GetAnimationById(animation_id));
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id));
}

// Ensure that we only generate FINISHED events for animations in a group
// once all animations in that group are finished.
TEST_F(ElementAnimationsTest, FinishedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  const int group_id = 1;

  // Add two animations with the same group id but different durations.
  std::unique_ptr<Animation> first_animation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 1,
      group_id, TargetProperty::TRANSFORM));
  first_animation->set_is_controlling_instance_for_test(true);
  animations_impl->AddAnimation(std::move(first_animation));

  std::unique_ptr<Animation> second_animation(Animation::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, group_id, TargetProperty::OPACITY));
  second_animation->set_is_controlling_instance_for_test(true);
  animations_impl->AddAnimation(std::move(second_animation));

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1000));
  animations_impl->UpdateState(true, events.get());

  // The opacity animation should be finished, but should not have generated
  // a FINISHED event yet.
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(Animation::FINISHED,
            animations_impl->GetAnimationById(2)->run_state());
  EXPECT_EQ(Animation::RUNNING,
            animations_impl->GetAnimationById(1)->run_state());

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(2000));
  animations_impl->UpdateState(true, events.get());

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

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  // Add two animations with the same group id.
  std::unique_ptr<Animation> first_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1,
      TargetProperty::TRANSFORM));
  first_animation->set_is_controlling_instance_for_test(true);
  animations_impl->AddAnimation(std::move(first_animation));

  std::unique_ptr<Animation> second_animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  second_animation->set_is_controlling_instance_for_test(true);
  animations_impl->AddAnimation(std::move(second_animation));

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  animations_impl->AbortAnimations(TargetProperty::OPACITY);

  events = host_impl_->CreateEvents();
  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1000));
  animations_impl->UpdateState(true, events.get());

  // We should have exactly 2 events: a FINISHED event for the tranform
  // animation, and an ABORTED event for the opacity animation.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(TargetProperty::TRANSFORM, events->events_[0].target_property);
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[1].type);
  EXPECT_EQ(TargetProperty::OPACITY, events->events_[1].target_property);
}

TEST_F(ElementAnimationsTest, HasAnimationThatAffectsScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  EXPECT_FALSE(animations_impl->HasAnimationThatAffectsScale());

  animations_impl->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  // Opacity animations don't affect scale.
  EXPECT_FALSE(animations_impl->HasAnimationThatAffectsScale());

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
  animations_impl->AddAnimation(std::move(animation));

  // Translations don't affect scale.
  EXPECT_FALSE(animations_impl->HasAnimationThatAffectsScale());

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
  animations_impl->AddAnimation(std::move(animation));

  EXPECT_TRUE(animations_impl->HasAnimationThatAffectsScale());

  animations_impl->GetAnimationById(3)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // HasAnimationThatAffectsScale.
  EXPECT_FALSE(animations_impl->HasAnimationThatAffectsScale());
}

TEST_F(ElementAnimationsTest, HasOnlyTranslationTransforms) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));

  animations_impl->AddAnimation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  // Opacity animations aren't non-translation transforms.
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));

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
  animations_impl->AddAnimation(std::move(animation));

  // The only transform animation we've added is a translation.
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));

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
  animations_impl->AddAnimation(std::move(animation));

  // A scale animation is not a translation.
  EXPECT_FALSE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_FALSE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));

  animations_impl->GetAnimationById(3)->set_affects_pending_elements(false);
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_FALSE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));

  animations_impl->GetAnimationById(3)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // HasOnlyTranslationTransforms.
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::PENDING));
  EXPECT_TRUE(
      animations_impl->HasOnlyTranslationTransforms(ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AnimationStartScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

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
  animations_impl->AddAnimation(std::move(animation));

  float start_scale = 0.f;
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::PENDING,
                                                   &start_scale));
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::ACTIVE,
                                                   &start_scale));
  EXPECT_EQ(0.f, start_scale);

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::PENDING,
                                                   &start_scale));
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::ACTIVE,
                                                   &start_scale));
  EXPECT_EQ(4.f, start_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations3;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3, nullptr));
  operations3.AppendScale(6.0, 5.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));

  animations_impl->RemoveAnimation(1);
  animation =
      Animation::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);

  // Reverse Direction
  animation->set_direction(Animation::Direction::REVERSE);
  animation->set_affects_active_elements(false);
  animations_impl->AddAnimation(std::move(animation));

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
  animations_impl->AddAnimation(std::move(animation));

  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::PENDING,
                                                   &start_scale));
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::ACTIVE,
                                                   &start_scale));
  EXPECT_EQ(0.f, start_scale);

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::PENDING,
                                                   &start_scale));
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::ACTIVE,
                                                   &start_scale));
  EXPECT_EQ(6.f, start_scale);

  animations_impl->GetAnimationById(2)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // AnimationStartScale.
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::PENDING,
                                                   &start_scale));
  EXPECT_EQ(5.f, start_scale);
  EXPECT_TRUE(animations_impl->AnimationStartScale(ElementListType::ACTIVE,
                                                   &start_scale));
  EXPECT_EQ(5.f, start_scale);
}

TEST_F(ElementAnimationsTest, MaximumTargetScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  float max_scale = 0.f;
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(0.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
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
  animations_impl->AddAnimation(std::move(animation));

  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(0.f, max_scale);

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
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
  animations_impl->AddAnimation(std::move(animation));

  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
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
  animations_impl->AddAnimation(std::move(animation));

  EXPECT_FALSE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                   &max_scale));
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                   &max_scale));
  EXPECT_FALSE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));

  animations_impl->GetAnimationById(3)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));
  animations_impl->GetAnimationById(2)->SetRunState(Animation::FINISHED,
                                                    TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // MaximumTargetScale.
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);
}

TEST_F(ElementAnimationsTest, MaximumTargetScaleWithDirection) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

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
  animations_impl->AddAnimation(std::move(animation_owned));

  float max_scale = 0.f;

  EXPECT_GT(animation->playback_rate(), 0.0);

  // NORMAL direction with positive playback rate.
  animation->set_direction(Animation::Direction::NORMAL);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // ALTERNATE direction with positive playback rate.
  animation->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // REVERSE direction with positive playback rate.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // ALTERNATE reverse direction.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  animation->set_playback_rate(-1.0);

  // NORMAL direction with negative playback rate.
  animation->set_direction(Animation::Direction::NORMAL);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // ALTERNATE direction with negative playback rate.
  animation->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // REVERSE direction with negative playback rate.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // ALTERNATE reverse direction with negative playback rate.
  animation->set_direction(Animation::Direction::REVERSE);
  EXPECT_TRUE(animations_impl->MaximumTargetScale(ElementListType::PENDING,
                                                  &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(
      animations_impl->MaximumTargetScale(ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);
}

TEST_F(ElementAnimationsTest, NewlyPushedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  EXPECT_FALSE(animations->needs_to_start_animations_for_testing());
  int animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1, 0.5f, 1.f, false);
  EXPECT_TRUE(animations->needs_to_start_animations_for_testing());

  EXPECT_FALSE(animations_impl->needs_to_start_animations_for_testing());
  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(animations_impl->needs_to_start_animations_for_testing());

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id)
                   ->affects_active_elements());

  animations_impl->Animate(kInitialTickTime);
  EXPECT_FALSE(animations_impl->needs_to_start_animations_for_testing());
  animations_impl->UpdateState(true, events.get());

  // Since the animation hasn't been activated, it should still be STARTING
  // rather than RUNNING.
  EXPECT_EQ(Animation::STARTING,
            animations_impl->GetAnimationById(animation_id)->run_state());

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_active_elements());

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1000));
  animations_impl->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state and the active observer should start to get ticked.
  EXPECT_EQ(Animation::RUNNING,
            animations_impl->GetAnimationById(animation_id)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ActivationBetweenAnimateAndUpdateState) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  const int animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1, 0.5f, 1.f, true);

  animations->PushPropertiesTo(animations_impl.get());

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id));
  EXPECT_EQ(Animation::WAITING_FOR_TARGET_AVAILABILITY,
            animations_impl->GetAnimationById(animation_id)->run_state());
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id)
                   ->affects_active_elements());

  animations_impl->Animate(kInitialTickTime);

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_active_elements());

  animations_impl->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state.
  EXPECT_EQ(Animation::RUNNING,
            animations_impl->GetAnimationById(animation_id)->run_state());

  animations_impl->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));

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

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

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
  AddAnimatedTransformToElementAnimations(animations.get(), 1.0, 1, 1);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());

  // animations_impl hasn't yet ticked at/past the end of the animation.
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1000));
  animations_impl->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int animation_id =
      AddAnimatedTransformToElementAnimations(animations.get(), 10.0, 2, 2);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(2000));
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  animations->RemoveAnimation(animation_id);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  animation_id =
      AddAnimatedTransformToElementAnimations(animations.get(), 10.0, 3, 3);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(2000));
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  animations_impl->AbortAnimations(TargetProperty::TRANSFORM);
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(4000));
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  animation_id =
      AddAnimatedTransformToElementAnimations(animations.get(), 1.0, 1, 6);
  animations->GetAnimationById(animation_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animations->GetAnimationById(animation_id)
      ->set_fill_mode(Animation::FillMode::NONE);

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenOpacityAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

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
  AddOpacityTransitionToElementAnimations(animations.get(), 1.0, 0.f, 1.f,
                                          false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animations->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());

  // animations_impl hasn't yet ticked at/past the end of the animation.
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1000));
  animations_impl->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 10.0, 0.f, 1.f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(2000));
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  animations->RemoveAnimation(animation_id);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 10.0, 0.f, 0.5f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(2000));
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationStarted(events->events_[0]);
  events->events_.clear();

  animations_impl->AbortAnimations(TargetProperty::OPACITY);
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(4000));
  animations_impl->UpdateState(true, events.get());

  animations->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1.0, 0.f, 0.5f, false /*use_timing_function*/);
  animations->GetAnimationById(animation_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animations->GetAnimationById(animation_id)
      ->set_fill_mode(Animation::FillMode::NONE);

  animations->PushPropertiesTo(animations_impl.get());
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  AddOpacityTransitionToElementAnimations(animations.get(), 1, 1.f, 2.f, true);

  animations->Animate(kInitialTickTime);
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedNegativeOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  AddOpacityTransitionToElementAnimations(animations.get(), 1, 0.f, -2.f, true);

  animations->Animate(kInitialTickTime);
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushedDeletedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  const int animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1, 0.5f, 1.f, true);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());
  EXPECT_EQ(Animation::RUNNING,
            animations_impl->GetAnimationById(animation_id)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_active_elements());

  // Delete the animation on the main-thread animations.
  animations->RemoveAnimation(
      animations->GetAnimation(TargetProperty::OPACITY)->id());
  animations->PushPropertiesTo(animations_impl.get());

  // The animation should no longer affect pending elements.
  EXPECT_FALSE(animations_impl->GetAnimationById(animation_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(animation_id)
                  ->affects_active_elements());

  animations_impl->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations_impl->UpdateState(true, events.get());

  // Only the active observer should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();

  // Activation should cause the animation to be deleted.
  EXPECT_FALSE(animations_impl->has_any_animation());
}

// Tests that an animation that affects only active elements won't block
// an animation that affects only pending elements from starting.
TEST_F(ElementAnimationsTest, StartAnimationsAffectingDifferentObservers) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  scoped_refptr<ElementAnimations> animations = element_animations();
  scoped_refptr<ElementAnimations> animations_impl = element_animations_impl();

  auto events = host_impl_->CreateEvents();

  const int first_animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1, 0.f, 1.f, true);

  animations->PushPropertiesTo(animations_impl.get());
  animations_impl->ActivateAnimations();
  animations_impl->Animate(kInitialTickTime);
  animations_impl->UpdateState(true, events.get());

  // Remove the first animation from the main-thread animations, and add a
  // new animation affecting the same property.
  animations->RemoveAnimation(
      animations->GetAnimation(TargetProperty::OPACITY)->id());
  const int second_animation_id = AddOpacityTransitionToElementAnimations(
      animations.get(), 1, 1.f, 0.5f, true);
  animations->PushPropertiesTo(animations_impl.get());

  // The original animation should only affect active elements, and the new
  // animation should only affect pending elements.
  EXPECT_FALSE(animations_impl->GetAnimationById(first_animation_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(first_animation_id)
                  ->affects_active_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(second_animation_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(animations_impl->GetAnimationById(second_animation_id)
                   ->affects_active_elements());

  animations_impl->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animations_impl->UpdateState(true, events.get());

  // The original animation should still be running, and the new animation
  // should be starting.
  EXPECT_EQ(Animation::RUNNING,
            animations_impl->GetAnimationById(first_animation_id)->run_state());
  EXPECT_EQ(
      Animation::STARTING,
      animations_impl->GetAnimationById(second_animation_id)->run_state());

  // The active observer should have been ticked by the original animation,
  // and the pending observer should have been ticked by the new animation.
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animations_impl->ActivateAnimations();

  // The original animation should have been deleted, and the new animation
  // should now affect both elements.
  EXPECT_FALSE(animations_impl->GetAnimationById(first_animation_id));
  EXPECT_TRUE(animations_impl->GetAnimationById(second_animation_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animations_impl->GetAnimationById(second_animation_id)
                  ->affects_active_elements());

  animations_impl->Animate(kInitialTickTime +
                           TimeDelta::FromMilliseconds(1000));
  animations_impl->UpdateState(true, events.get());

  // The new animation should be running, and the active observer should have
  // been ticked at the new animation's starting point.
  EXPECT_EQ(
      Animation::RUNNING,
      animations_impl->GetAnimationById(second_animation_id)->run_state());
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(1.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsCurrentlyAnimatingProperty) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  // Create an animation that initially affects only pending elements.
  std::unique_ptr<Animation> animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animation->set_affects_active_elements(false);

  animations->AddAnimation(std::move(animation));
  animations->Animate(kInitialTickTime);
  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(animations->HasActiveAnimation());

  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animations->ActivateAnimations();

  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(10));
  animations->UpdateState(true, nullptr);

  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Tick past the end of the animation.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(1100));
  animations->UpdateState(true, nullptr);

  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsAnimatingPropertyTimeOffsetFillMode) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  scoped_refptr<ElementAnimations> animations = element_animations();

  // Create an animation that initially affects only pending elements, and has
  // a start delay of 2 seconds.
  std::unique_ptr<Animation> animation(CreateAnimation(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animation->set_fill_mode(Animation::FillMode::NONE);
  animation->set_time_offset(TimeDelta::FromMilliseconds(-2000));
  animation->set_affects_active_elements(false);

  animations->AddAnimation(std::move(animation));

  animations->Animate(kInitialTickTime);

  // Since the animation has a start delay, the elements it affects have a
  // potentially running transform animation but aren't currently animating
  // transform.
  EXPECT_TRUE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animations->ActivateAnimations();

  EXPECT_TRUE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(animations->HasActiveAnimation());
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animations->UpdateState(true, nullptr);

  // Tick past the start delay.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animations->UpdateState(true, nullptr);
  EXPECT_TRUE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));

  // After the animaton finishes, the elements it affects have neither a
  // potentially running transform animation nor a currently running transform
  // animation.
  animations->Animate(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animations->UpdateState(true, nullptr);
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animations->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
}

}  // namespace
}  // namespace cc
