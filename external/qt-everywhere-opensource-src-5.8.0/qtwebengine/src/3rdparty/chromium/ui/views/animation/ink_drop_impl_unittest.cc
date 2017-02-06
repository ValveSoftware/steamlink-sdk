// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_impl.h"

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/animation/test/test_ink_drop_host.h"

namespace views {

// NOTE: The InkDropImpl class is also tested by the InkDropFactoryTest tests.
class InkDropImplTest : public testing::Test {
 public:
  InkDropImplTest();
  ~InkDropImplTest() override;

 protected:
  TestInkDropHost ink_drop_host_;

  // The test target.
  InkDropImpl ink_drop_;

  // Allows privileged access to the the |ink_drop_highlight_|.
  test::InkDropImplTestApi test_api_;

  // Used to control the tasks scheduled by the InkDropImpl's Timer.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // Required by base::Timer's.
  std::unique_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropImplTest);
};

InkDropImplTest::InkDropImplTest()
    : ink_drop_(&ink_drop_host_),
      test_api_(&ink_drop_),
      task_runner_(new base::TestSimpleTaskRunner),
      thread_task_runner_handle_(
          new base::ThreadTaskRunnerHandle(task_runner_)) {
  ink_drop_host_.set_disable_timers_for_test(true);
}

InkDropImplTest::~InkDropImplTest() {}

TEST_F(InkDropImplTest, SetHoveredHighlightIsFadingInOrVisible) {
  ink_drop_host_.set_should_show_highlight(true);

  ink_drop_.SetHovered(true);
  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  test_api_.CompleteAnimations();

  ink_drop_.SetHovered(false);
  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());
}

TEST_F(InkDropImplTest, FocusAndHoverAtSameTime) {
  ink_drop_host_.set_should_show_highlight(true);
  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());

  ink_drop_.SetFocused(true);
  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  test_api_.CompleteAnimations();
  ink_drop_.SetHovered(false);
  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  test_api_.CompleteAnimations();
  ink_drop_.SetHovered(true);
  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  test_api_.CompleteAnimations();
  ink_drop_.SetFocused(false);
  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  test_api_.CompleteAnimations();
  ink_drop_.SetHovered(false);
  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());
}

TEST_F(InkDropImplTest, HighlightDoesntFadeInAfterAnimationIfHighlightNotSet) {
  ink_drop_host_.set_should_show_highlight(true);
  ink_drop_.SetHovered(false);
  ink_drop_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api_.CompleteAnimations();

  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());
}

TEST_F(InkDropImplTest, HighlightFadesInAfterAnimationWhenHostIsHovered) {
  ink_drop_host_.set_should_show_highlight(true);
  ink_drop_.SetHovered(true);
  ink_drop_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api_.CompleteAnimations();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunPendingTasks();

  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());
}

TEST_F(InkDropImplTest,
       HighlightDoesntFadeInAfterAnimationWhenHostIsNotHovered) {
  ink_drop_host_.set_should_show_highlight(false);
  ink_drop_.SetHovered(true);
  ink_drop_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api_.CompleteAnimations();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunPendingTasks();

  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());
}

TEST_F(InkDropImplTest, HoveredStateNotVisibleOrFadingInAfterAnimateToState) {
  ink_drop_host_.set_should_show_highlight(true);

  ink_drop_.SetHovered(true);
  test_api_.CompleteAnimations();
  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  ink_drop_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());
}

// Verifies that there is not a crash when setting hovered state and the host
// returns null for the highlight.
TEST_F(InkDropImplTest, SetHoveredFalseWorksWhenNoInkDropHighlightExists) {
  ink_drop_host_.set_should_show_highlight(false);
  ink_drop_.SetHovered(true);
  EXPECT_FALSE(test_api_.highlight());
  ink_drop_.SetHovered(false);
  EXPECT_FALSE(test_api_.highlight());
}

TEST_F(InkDropImplTest, HighlightFadesOutOnSnapToActivated) {
  ink_drop_host_.set_should_show_highlight(true);
  ink_drop_.SetHovered(true);
  test_api_.CompleteAnimations();

  EXPECT_TRUE(test_api_.IsHighlightFadingInOrVisible());

  ink_drop_.SnapToActivated();

  EXPECT_FALSE(test_api_.IsHighlightFadingInOrVisible());
}

TEST_F(InkDropImplTest, LayersRemovedFromHostAfterHighlight) {
  ink_drop_host_.set_should_show_highlight(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();

  ink_drop_.SetHovered(false);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropImplTest, LayersRemovedFromHostAfterInkDrop) {
  ink_drop_host_.set_should_show_highlight(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropImplTest, LayersAddedToHostWhenHighlightOrInkDropVisible) {
  ink_drop_host_.set_should_show_highlight(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunPendingTasks();

  // Highlight should be fading back in.
  EXPECT_TRUE(test_api_.HasActiveAnimations());
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropImplTest, LayersNotAddedToHostWhenHighlightTimeFires) {
  ink_drop_host_.set_should_show_highlight(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.AnimateToState(InkDropState::HIDDEN);
  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_host_.set_should_show_highlight(false);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunPendingTasks();

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropImplTest, LayersArentRemovedWhenPreemptingFadeOut) {
  ink_drop_host_.set_should_show_highlight(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();

  ink_drop_.SetHovered(false);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());
}

}  // namespace views
