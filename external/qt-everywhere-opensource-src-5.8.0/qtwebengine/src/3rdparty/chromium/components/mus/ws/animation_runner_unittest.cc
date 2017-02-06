// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/animation_runner.h"

#include "base/strings/stringprintf.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/ws/animation_runner_observer.h"
#include "components/mus/ws/scheduled_animation_group.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace mus {
using mojom::AnimationProperty;
using mojom::AnimationTweenType;
using mojom::AnimationElement;
using mojom::AnimationGroup;
using mojom::AnimationProperty;
using mojom::AnimationSequence;
using mojom::AnimationTweenType;
using mojom::AnimationValue;
using mojom::AnimationValuePtr;

namespace ws {
namespace {

class TestAnimationRunnerObserver : public AnimationRunnerObserver {
 public:
  TestAnimationRunnerObserver() {}
  ~TestAnimationRunnerObserver() override {}

  std::vector<std::string>* changes() { return &changes_; }
  std::vector<uint32_t>* change_ids() { return &change_ids_; }

  void clear_changes() {
    changes_.clear();
    change_ids_.clear();
  }

  // AnimationRunnerDelgate:
  void OnAnimationScheduled(uint32_t id) override {
    change_ids_.push_back(id);
    changes_.push_back("scheduled");
  }
  void OnAnimationDone(uint32_t id) override {
    change_ids_.push_back(id);
    changes_.push_back("done");
  }
  void OnAnimationInterrupted(uint32_t id) override {
    change_ids_.push_back(id);
    changes_.push_back("interrupted");
  }
  void OnAnimationCanceled(uint32_t id) override {
    change_ids_.push_back(id);
    changes_.push_back("canceled");
  }

 private:
  std::vector<uint32_t> change_ids_;
  std::vector<std::string> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestAnimationRunnerObserver);
};

// Creates an AnimationValuePtr from the specified float value.
AnimationValuePtr FloatAnimationValue(float float_value) {
  AnimationValuePtr value(AnimationValue::New());
  value->float_value = float_value;
  return value;
}

// Creates an AnimationValuePtr from the specified transform.
AnimationValuePtr TransformAnimationValue(const gfx::Transform& transform) {
  AnimationValuePtr value(AnimationValue::New());
  value->transform = transform;
  return value;
}

// Adds an AnimationElement to |group|s last sequence with the specified value.
void AddElement(AnimationGroup* group,
                TimeDelta time,
                AnimationValuePtr start_value,
                AnimationValuePtr target_value,
                AnimationProperty property,
                AnimationTweenType tween_type) {
  AnimationSequence& sequence =
      *(group->sequences[group->sequences.size() - 1]);
  sequence.elements.push_back(AnimationElement::New());
  AnimationElement& element =
      *(sequence.elements[sequence.elements.size() - 1]);
  element.property = property;
  element.duration = time.InMicroseconds();
  element.tween_type = tween_type;
  element.start_value = std::move(start_value);
  element.target_value = std::move(target_value);
}

void AddOpacityElement(AnimationGroup* group,
                       TimeDelta time,
                       AnimationValuePtr start_value,
                       AnimationValuePtr target_value) {
  AddElement(group, time, std::move(start_value), std::move(target_value),
             AnimationProperty::OPACITY, AnimationTweenType::LINEAR);
}

void AddTransformElement(AnimationGroup* group,
                         TimeDelta time,
                         AnimationValuePtr start_value,
                         AnimationValuePtr target_value) {
  AddElement(group, time, std::move(start_value), std::move(target_value),
             AnimationProperty::TRANSFORM, AnimationTweenType::LINEAR);
}

void AddPauseElement(AnimationGroup* group, TimeDelta time) {
  AddElement(group, time, AnimationValuePtr(), AnimationValuePtr(),
             AnimationProperty::NONE, AnimationTweenType::LINEAR);
}

void InitGroupForWindow(AnimationGroup* group,
                        const WindowId& id,
                        int cycle_count) {
  group->window_id = WindowIdToTransportId(id);
  group->sequences.push_back(AnimationSequence::New());
  group->sequences[group->sequences.size() - 1]->cycle_count = cycle_count;
}

}  // namespace

class AnimationRunnerTest : public testing::Test {
 public:
  AnimationRunnerTest()
      : initial_time_(base::TimeTicks::Now()), runner_(initial_time_) {
    runner_.AddObserver(&runner_observer_);
  }
  ~AnimationRunnerTest() override { runner_.RemoveObserver(&runner_observer_); }

 protected:
  // Convenience to schedule an animation for a single window/group pair.
  AnimationRunner::AnimationId ScheduleForSingleWindow(
      ServerWindow* window,
      const AnimationGroup* group,
      base::TimeTicks now) {
    std::vector<AnimationRunner::WindowAndAnimationPair> pairs;
    pairs.push_back(std::make_pair(window, group));
    return runner_.Schedule(pairs, now);
  }

  // If |id| is valid and there is only one window schedule against the
  // animation it is returned; otherwise returns null.
  ServerWindow* GetSingleWindowAnimating(AnimationRunner::AnimationId id) {
    std::set<ServerWindow*> windows(runner_.GetWindowsAnimating(id));
    return windows.size() == 1 ? *windows.begin() : nullptr;
  }

  const base::TimeTicks initial_time_;
  TestAnimationRunnerObserver runner_observer_;
  AnimationRunner runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnimationRunnerTest);
};

// Opacity from 1 to .5 over 1000.
TEST_F(AnimationRunnerTest, SingleProperty) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId());

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  const uint32_t animation_id =
      ScheduleForSingleWindow(&window, &group, initial_time_);

  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("scheduled", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));
  runner_observer_.clear_changes();

  EXPECT_TRUE(runner_.HasAnimations());

  // Opacity should still be 1 (the initial value).
  EXPECT_EQ(1.f, window.opacity());

  // Animate half way.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));

  EXPECT_EQ(.75f, window.opacity());
  EXPECT_TRUE(runner_observer_.changes()->empty());

  // Run well past the end. Value should progress to end and delegate should
  // be notified.
  runner_.Tick(initial_time_ + TimeDelta::FromSeconds(10));
  EXPECT_EQ(.5f, window.opacity());

  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("done", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));

  EXPECT_FALSE(runner_.HasAnimations());
}

// Opacity from 1 to .5, followed by transform from identity to 2x,3x.
TEST_F(AnimationRunnerTest, TwoPropertiesInSequence) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId());

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(.5f));

  gfx::Transform done_transform;
  done_transform.Scale(2, 4);
  AddTransformElement(&group, TimeDelta::FromMicroseconds(2000),
                      AnimationValuePtr(),
                      TransformAnimationValue(done_transform));

  const uint32_t animation_id =
      ScheduleForSingleWindow(&window, &group, initial_time_);
  runner_observer_.clear_changes();

  // Nothing in the window should have changed yet.
  EXPECT_EQ(1.f, window.opacity());
  EXPECT_TRUE(window.transform().IsIdentity());

  // Animate half way from through opacity animation.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));

  EXPECT_EQ(.75f, window.opacity());
  EXPECT_TRUE(window.transform().IsIdentity());

  // Finish first element (opacity).
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1000));
  EXPECT_EQ(.5f, window.opacity());
  EXPECT_TRUE(window.transform().IsIdentity());

  // Half way through second (transform).
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(2000));
  EXPECT_EQ(.5f, window.opacity());
  gfx::Transform half_way_transform;
  half_way_transform.Scale(1.5, 2.5);
  EXPECT_EQ(half_way_transform, window.transform());

  EXPECT_TRUE(runner_observer_.changes()->empty());

  // To end.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(3500));
  EXPECT_EQ(.5f, window.opacity());
  EXPECT_EQ(done_transform, window.transform());

  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("done", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));
}

// Opacity from .5 to 1 over 1000, transform to 2x,4x over 500.
TEST_F(AnimationRunnerTest, TwoPropertiesInParallel) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId(1, 1));

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    FloatAnimationValue(.5f), FloatAnimationValue(1));

  group.sequences.push_back(AnimationSequence::New());
  group.sequences[1]->cycle_count = 1;
  gfx::Transform done_transform;
  done_transform.Scale(2, 4);
  AddTransformElement(&group, TimeDelta::FromMicroseconds(500),
                      AnimationValuePtr(),
                      TransformAnimationValue(done_transform));

  const uint32_t animation_id =
      ScheduleForSingleWindow(&window, &group, initial_time_);

  runner_observer_.clear_changes();

  // Nothing in the window should have changed yet.
  EXPECT_EQ(1.f, window.opacity());
  EXPECT_TRUE(window.transform().IsIdentity());

  // Animate to 250, which is 1/4 way through opacity and half way through
  // transform.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(250));

  EXPECT_EQ(.625f, window.opacity());
  gfx::Transform half_way_transform;
  half_way_transform.Scale(1.5, 2.5);
  EXPECT_EQ(half_way_transform, window.transform());

  // Animate to 500, which is 1/2 way through opacity and transform done.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));
  EXPECT_EQ(.75f, window.opacity());
  EXPECT_EQ(done_transform, window.transform());

  // Animate to 750, which is 3/4 way through opacity and transform done.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(750));
  EXPECT_EQ(.875f, window.opacity());
  EXPECT_EQ(done_transform, window.transform());

  EXPECT_TRUE(runner_observer_.changes()->empty());

  // To end.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(3500));
  EXPECT_EQ(1.f, window.opacity());
  EXPECT_EQ(done_transform, window.transform());

  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("done", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));
}

// Opacity from .5 to 1 over 1000, pause for 500, 1 to .5 over 500, with a cycle
// count of 3.
TEST_F(AnimationRunnerTest, Cycles) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId(1, 2));

  window.SetOpacity(.5f);

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 3);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(1));
  AddPauseElement(&group, TimeDelta::FromMicroseconds(500));
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(500),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  ScheduleForSingleWindow(&window, &group, initial_time_);
  runner_observer_.clear_changes();

  // Nothing in the window should have changed yet.
  EXPECT_EQ(.5f, window.opacity());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));
  EXPECT_EQ(.75f, window.opacity());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1250));
  EXPECT_EQ(1.f, window.opacity());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1750));
  EXPECT_EQ(.75f, window.opacity());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(2500));
  EXPECT_EQ(.75f, window.opacity());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(3250));
  EXPECT_EQ(1.f, window.opacity());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(3750));
  EXPECT_EQ(.75f, window.opacity());

  // Animate to the end.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(6500));
  EXPECT_EQ(.5f, window.opacity());

  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("done", runner_observer_.changes()->at(0));
}

// Verifies scheduling the same window twice sends an interrupt.
TEST_F(AnimationRunnerTest, ScheduleTwice) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId(1, 2));

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  const uint32_t animation_id =
      ScheduleForSingleWindow(&window, &group, initial_time_);
  runner_observer_.clear_changes();

  // Animate half way.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));

  EXPECT_EQ(.75f, window.opacity());
  EXPECT_TRUE(runner_observer_.changes()->empty());

  // Schedule again. We should get an interrupt, but opacity shouldn't change.
  const uint32_t animation2_id = ScheduleForSingleWindow(
      &window, &group, initial_time_ + TimeDelta::FromMicroseconds(500));

  // Id should have changed.
  EXPECT_NE(animation_id, animation2_id);

  EXPECT_FALSE(runner_.IsAnimating(animation_id));
  EXPECT_EQ(&window, GetSingleWindowAnimating(animation2_id));

  EXPECT_EQ(.75f, window.opacity());
  EXPECT_EQ(2u, runner_observer_.changes()->size());
  EXPECT_EQ("interrupted", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));
  EXPECT_EQ("scheduled", runner_observer_.changes()->at(1));
  EXPECT_EQ(animation2_id, runner_observer_.change_ids()->at(1));
  runner_observer_.clear_changes();

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1000));
  EXPECT_EQ(.625f, window.opacity());
  EXPECT_TRUE(runner_observer_.changes()->empty());

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(2000));
  EXPECT_EQ(.5f, window.opacity());
  EXPECT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("done", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation2_id, runner_observer_.change_ids()->at(0));
}

// Verifies Remove() works.
TEST_F(AnimationRunnerTest, CancelAnimationForWindow) {
  // Create an animation and advance it part way.
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId());
  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  const uint32_t animation_id =
      ScheduleForSingleWindow(&window, &group, initial_time_);
  runner_observer_.clear_changes();
  EXPECT_EQ(&window, GetSingleWindowAnimating(animation_id));

  EXPECT_TRUE(runner_.HasAnimations());

  // Animate half way.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));
  EXPECT_EQ(.75f, window.opacity());
  EXPECT_TRUE(runner_observer_.changes()->empty());

  // Cancel the animation.
  runner_.CancelAnimationForWindow(&window);

  EXPECT_FALSE(runner_.HasAnimations());
  EXPECT_EQ(nullptr, GetSingleWindowAnimating(animation_id));

  EXPECT_EQ(.75f, window.opacity());

  EXPECT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("canceled", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));
}

// Verifies a tick with a very large delta and a sequence that repeats forever
// doesn't take a long time.
TEST_F(AnimationRunnerTest, InfiniteRepeatWithHugeGap) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId(1, 2));

  window.SetOpacity(.5f);

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 0);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(500),
                    AnimationValuePtr(), FloatAnimationValue(1));
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(500),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  ScheduleForSingleWindow(&window, &group, initial_time_);
  runner_observer_.clear_changes();

  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1000000000750));

  EXPECT_EQ(.75f, window.opacity());

  ASSERT_EQ(0u, runner_observer_.changes()->size());
}

// Verifies a second schedule sets any properties that are no longer animating
// to their final value.
TEST_F(AnimationRunnerTest, RescheduleSetsPropertiesToFinalValue) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId());

  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  gfx::Transform done_transform;
  done_transform.Scale(2, 4);
  AddTransformElement(&group, TimeDelta::FromMicroseconds(500),
                      AnimationValuePtr(),
                      TransformAnimationValue(done_transform));

  ScheduleForSingleWindow(&window, &group, initial_time_);

  // Schedule() again, this time without animating opacity.
  group.sequences[0]->elements[0]->property = AnimationProperty::NONE;
  ScheduleForSingleWindow(&window, &group, initial_time_);

  // Opacity should go to final value.
  EXPECT_EQ(.5f, window.opacity());
  // Transform shouldn't have changed since newly scheduled animation also has
  // transform in it.
  EXPECT_TRUE(window.transform().IsIdentity());
}

// Opacity from 1 to .5 over 1000 of v1 and v2 transform to 2x,4x over 500.
TEST_F(AnimationRunnerTest, TwoWindows) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window1(&window_delegate, WindowId());
  ServerWindow window2(&window_delegate, WindowId(1, 2));

  AnimationGroup group1;
  InitGroupForWindow(&group1, window1.id(), 1);
  AddOpacityElement(&group1, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(.5));

  AnimationGroup group2;
  InitGroupForWindow(&group2, window2.id(), 1);
  gfx::Transform done_transform;
  done_transform.Scale(2, 4);
  AddTransformElement(&group2, TimeDelta::FromMicroseconds(500),
                      AnimationValuePtr(),
                      TransformAnimationValue(done_transform));

  std::vector<AnimationRunner::WindowAndAnimationPair> pairs;
  pairs.push_back(std::make_pair(&window1, &group1));
  pairs.push_back(std::make_pair(&window2, &group2));

  const uint32_t animation_id = runner_.Schedule(pairs, initial_time_);

  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("scheduled", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id, runner_observer_.change_ids()->at(0));
  runner_observer_.clear_changes();

  EXPECT_TRUE(runner_.HasAnimations());
  EXPECT_TRUE(runner_.IsAnimating(animation_id));

  // Properties should be at the initial value.
  EXPECT_EQ(1.f, window1.opacity());
  EXPECT_TRUE(window2.transform().IsIdentity());

  // Animate 250ms in, which is quarter way for opacity and half way for
  // transform.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(250));
  EXPECT_EQ(.875f, window1.opacity());
  gfx::Transform half_way_transform;
  half_way_transform.Scale(1.5, 2.5);
  EXPECT_EQ(half_way_transform, window2.transform());
  std::set<ServerWindow*> windows_animating(
      runner_.GetWindowsAnimating(animation_id));
  EXPECT_EQ(2u, windows_animating.size());
  EXPECT_EQ(1u, windows_animating.count(&window1));
  EXPECT_EQ(1u, windows_animating.count(&window2));

  // Animate 750ms in, window1 should be done 3/4 done, and window2 done.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(750));
  EXPECT_EQ(.625, window1.opacity());
  EXPECT_EQ(done_transform, window2.transform());
  windows_animating = runner_.GetWindowsAnimating(animation_id);
  EXPECT_EQ(1u, windows_animating.size());
  EXPECT_EQ(1u, windows_animating.count(&window1));
  EXPECT_TRUE(runner_.HasAnimations());
  EXPECT_TRUE(runner_.IsAnimating(animation_id));

  // Animate to end.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1750));
  EXPECT_EQ(.5, window1.opacity());
  EXPECT_EQ(done_transform, window2.transform());
  windows_animating = runner_.GetWindowsAnimating(animation_id);
  EXPECT_TRUE(windows_animating.empty());
  EXPECT_FALSE(runner_.HasAnimations());
  EXPECT_FALSE(runner_.IsAnimating(animation_id));
}

TEST_F(AnimationRunnerTest, Reschedule) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId());

  // Animation from 1-0 over 1ms and in parallel transform to 2x,4x over 1ms.
  AnimationGroup group;
  InitGroupForWindow(&group, window.id(), 1);
  AddOpacityElement(&group, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(0));
  group.sequences.push_back(AnimationSequence::New());
  group.sequences[1]->cycle_count = 1;
  gfx::Transform done_transform;
  done_transform.Scale(2, 4);
  AddTransformElement(&group, TimeDelta::FromMicroseconds(1000),
                      AnimationValuePtr(),
                      TransformAnimationValue(done_transform));
  const uint32_t animation_id1 =
      ScheduleForSingleWindow(&window, &group, initial_time_);

  // Animate half way in.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(500));
  EXPECT_EQ(.5f, window.opacity());
  gfx::Transform half_way_transform;
  half_way_transform.Scale(1.5, 2.5);
  EXPECT_EQ(half_way_transform, window.transform());

  runner_observer_.clear_changes();

  // Schedule the same window animating opacity to 1.
  AnimationGroup group2;
  InitGroupForWindow(&group2, window.id(), 1);
  AddOpacityElement(&group2, TimeDelta::FromMicroseconds(1000),
                    AnimationValuePtr(), FloatAnimationValue(1));
  const uint32_t animation_id2 = ScheduleForSingleWindow(
      &window, &group2, initial_time_ + TimeDelta::FromMicroseconds(500));

  // Opacity should remain at .5, but transform should go to end state.
  EXPECT_EQ(.5f, window.opacity());
  EXPECT_EQ(done_transform, window.transform());

  ASSERT_EQ(2u, runner_observer_.changes()->size());
  EXPECT_EQ("interrupted", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id1, runner_observer_.change_ids()->at(0));
  EXPECT_EQ("scheduled", runner_observer_.changes()->at(1));
  EXPECT_EQ(animation_id2, runner_observer_.change_ids()->at(1));
  runner_observer_.clear_changes();

  // Animate half way through new sequence. Opacity should be the only thing
  // changing.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(1000));
  EXPECT_EQ(.75f, window.opacity());
  EXPECT_EQ(done_transform, window.transform());
  ASSERT_EQ(0u, runner_observer_.changes()->size());

  // Animate to end.
  runner_.Tick(initial_time_ + TimeDelta::FromMicroseconds(2000));
  ASSERT_EQ(1u, runner_observer_.changes()->size());
  EXPECT_EQ("done", runner_observer_.changes()->at(0));
  EXPECT_EQ(animation_id2, runner_observer_.change_ids()->at(0));
}

}  // ws
}  // mus
