// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_highlight.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/animation/test/test_ink_drop_highlight_observer.h"

namespace views {
namespace test {

class InkDropHighlightTest : public testing::Test {
 public:
  InkDropHighlightTest();
  ~InkDropHighlightTest() override;

 protected:
  InkDropHighlight* ink_drop_highlight() { return ink_drop_highlight_.get(); }

  InkDropHighlightTestApi* test_api() { return test_api_.get(); }

  // Observer of the test target.
  TestInkDropHighlightObserver* observer() { return &observer_; }

  // Initializes |ink_drop_highlight_| and attaches |test_api_| and |observer_|
  // to the new instance.
  void InitHighlight(std::unique_ptr<InkDropHighlight> new_highlight);

  // Destroys the |ink_drop_highlight_| and the attached |test_api_|.
  void DestroyHighlight();

 private:
  // The test target.
  std::unique_ptr<InkDropHighlight> ink_drop_highlight_;

  // Allows privileged access to the the |ink_drop_highlight_|.
  std::unique_ptr<InkDropHighlightTestApi> test_api_;

  // Observer of the test target.
  TestInkDropHighlightObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHighlightTest);
};

InkDropHighlightTest::InkDropHighlightTest() {
  InitHighlight(base::MakeUnique<InkDropHighlight>(
      gfx::Size(10, 10), 3, gfx::PointF(), SK_ColorBLACK));
}

InkDropHighlightTest::~InkDropHighlightTest() {
  // Destory highlight to make sure it is destroyed before the observer.
  DestroyHighlight();
}

void InkDropHighlightTest::InitHighlight(
    std::unique_ptr<InkDropHighlight> new_highlight) {
  ink_drop_highlight_ = std::move(new_highlight);
  test_api_ =
      base::MakeUnique<InkDropHighlightTestApi>(ink_drop_highlight_.get());
  test_api()->SetDisableAnimationTimers(true);
  ink_drop_highlight()->set_observer(&observer_);
}

void InkDropHighlightTest::DestroyHighlight() {
  test_api_.reset();
  ink_drop_highlight_.reset();
}

TEST_F(InkDropHighlightTest, InitialStateAfterConstruction) {
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());
}

TEST_F(InkDropHighlightTest, IsHighlightedStateTransitions) {
  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(ink_drop_highlight()->IsFadingInOrVisible());

  test_api()->CompleteAnimations();
  EXPECT_TRUE(ink_drop_highlight()->IsFadingInOrVisible());

  ink_drop_highlight()->FadeOut(base::TimeDelta::FromSeconds(1),
                                false /* explode */);
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());
}

TEST_F(InkDropHighlightTest, VerifyObserversAreNotified) {
  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(1, observer()->last_animation_started_ordinal());
  EXPECT_FALSE(observer()->AnimationHasEnded());

  test_api()->CompleteAnimations();

  EXPECT_TRUE(observer()->AnimationHasEnded());
  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
}

TEST_F(InkDropHighlightTest,
       VerifyObserversAreNotifiedWithCorrectAnimationType) {
  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(observer()->AnimationHasStarted());
  EXPECT_EQ(InkDropHighlight::FADE_IN,
            observer()->last_animation_started_context());

  test_api()->CompleteAnimations();
  EXPECT_TRUE(observer()->AnimationHasEnded());
  EXPECT_EQ(InkDropHighlight::FADE_IN,
            observer()->last_animation_started_context());

  ink_drop_highlight()->FadeOut(base::TimeDelta::FromSeconds(1),
                                false /* explode */);
  EXPECT_EQ(InkDropHighlight::FADE_OUT,
            observer()->last_animation_started_context());

  test_api()->CompleteAnimations();
  EXPECT_EQ(InkDropHighlight::FADE_OUT,
            observer()->last_animation_started_context());
}

TEST_F(InkDropHighlightTest, VerifyObserversAreNotifiedOfSuccessfulAnimations) {
  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));
  test_api()->CompleteAnimations();

  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer()->last_animation_ended_reason());
}

TEST_F(InkDropHighlightTest, VerifyObserversAreNotifiedOfPreemptedAnimations) {
  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));
  ink_drop_highlight()->FadeOut(base::TimeDelta::FromSeconds(1),
                                false /* explode */);

  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
  EXPECT_EQ(InkDropHighlight::FADE_IN,
            observer()->last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer()->last_animation_ended_reason());
}

// Confirms there is no crash.
TEST_F(InkDropHighlightTest, NullObserverIsSafe) {
  ink_drop_highlight()->set_observer(nullptr);

  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));
  test_api()->CompleteAnimations();

  ink_drop_highlight()->FadeOut(base::TimeDelta::FromMilliseconds(0),
                                false /* explode */);
  test_api()->CompleteAnimations();
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());
}

// Verify animations are aborted during deletion and the
// InkDropHighlightObservers are notified.
TEST_F(InkDropHighlightTest, AnimationsAbortedDuringDeletion) {
  ink_drop_highlight()->FadeIn(base::TimeDelta::FromSeconds(1));
  DestroyHighlight();
  EXPECT_EQ(1, observer()->last_animation_started_ordinal());
  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
  EXPECT_EQ(InkDropHighlight::FADE_IN,
            observer()->last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer()->last_animation_ended_reason());
}

// Confirms a zero sized highlight doesn't crash.
TEST_F(InkDropHighlightTest, AnimatingAZeroSizeHighlight) {
  InitHighlight(base::MakeUnique<InkDropHighlight>(
      gfx::Size(0, 0), 3, gfx::PointF(), SK_ColorBLACK));
  ink_drop_highlight()->FadeOut(base::TimeDelta::FromMilliseconds(0),
                                false /* explode */);
}

}  // namespace test
}  // namespace views
