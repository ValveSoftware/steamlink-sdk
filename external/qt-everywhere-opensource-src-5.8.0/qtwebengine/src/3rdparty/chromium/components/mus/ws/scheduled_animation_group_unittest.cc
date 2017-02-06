// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/scheduled_animation_group.h"

#include "components/mus/public/interfaces/animations.mojom.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using mus::mojom::AnimationProperty;
using mus::mojom::AnimationTweenType;
using mus::mojom::AnimationGroup;
using mus::mojom::AnimationSequence;
using mus::mojom::AnimationElement;
using mus::mojom::AnimationValue;

namespace mus {
namespace ws {
namespace {

bool IsAnimationGroupValid(const AnimationGroup& transport_group) {
  TestServerWindowDelegate window_delegate;
  ServerWindow window(&window_delegate, WindowId());
  std::unique_ptr<ScheduledAnimationGroup> group(
      ScheduledAnimationGroup::Create(&window, base::TimeTicks::Now(), 1,
                                      transport_group));
  return group.get() != nullptr;
}

}  // namespace

TEST(ScheduledAnimationGroupTest, IsAnimationGroupValid) {
  AnimationGroup group;

  // AnimationGroup with no sequences is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  group.sequences.push_back(AnimationSequence::New());

  // Sequence with no elements is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  AnimationSequence& sequence = *(group.sequences[0]);
  sequence.elements.push_back(AnimationElement::New());
  AnimationElement& element = *(sequence.elements[0]);
  element.property = AnimationProperty::OPACITY;
  element.tween_type = AnimationTweenType::LINEAR;

  // Element with no target_value is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  // Opacity must be between 0 and 1.
  element.target_value = AnimationValue::New();
  element.target_value->float_value = 2.5f;
  EXPECT_FALSE(IsAnimationGroupValid(group));

  element.target_value->float_value = .5f;
  EXPECT_TRUE(IsAnimationGroupValid(group));

  // Bogus start value.
  element.start_value = AnimationValue::New();
  element.start_value->float_value = 2.5f;
  EXPECT_FALSE(IsAnimationGroupValid(group));

  element.start_value->float_value = .5f;
  EXPECT_TRUE(IsAnimationGroupValid(group));

  // All transforms are valid.
  element.property = AnimationProperty::TRANSFORM;
  EXPECT_TRUE(IsAnimationGroupValid(group));

  // Add another empty sequence, should be invalid again.
  group.sequences.push_back(AnimationSequence::New());
  EXPECT_FALSE(IsAnimationGroupValid(group));

  AnimationSequence& sequence2 = *(group.sequences[1]);
  sequence2.elements.push_back(AnimationElement::New());
  AnimationElement& element2 = *(sequence2.elements[0]);
  element2.property = AnimationProperty::OPACITY;
  element2.tween_type = AnimationTweenType::LINEAR;

  // Element with no target_value is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  element2.property = AnimationProperty::NONE;
  EXPECT_TRUE(IsAnimationGroupValid(group));
}

}  // namespace ws
}  // namespace mus
