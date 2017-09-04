/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the ScrollAnimator class.

#include "platform/scroll/ScrollAnimator.h"

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollableArea.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using testing::AtLeast;
using testing::Return;
using testing::_;

static double gMockedTime = 0.0;

static double getMockedTime() {
  return gMockedTime;
}

namespace {

class MockScrollableArea : public GarbageCollectedFinalized<MockScrollableArea>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(MockScrollableArea);

 public:
  static MockScrollableArea* create(bool scrollAnimatorEnabled,
                                    const ScrollOffset& minOffset,
                                    const ScrollOffset& maxOffset) {
    return new MockScrollableArea(scrollAnimatorEnabled, minOffset, maxOffset);
  }

  MOCK_CONST_METHOD0(visualRectForScrollbarParts, LayoutRect());
  MOCK_CONST_METHOD0(isActive, bool());
  MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
  MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
  MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
  MOCK_METHOD2(updateScrollOffset, void(const ScrollOffset&, ScrollType));
  MOCK_METHOD0(scrollControlWasSetNeedsPaintInvalidation, void());
  MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
  MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
  MOCK_CONST_METHOD0(contentsSize, IntSize());
  MOCK_CONST_METHOD0(scrollbarsCanBeActive, bool());
  MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());
  MOCK_METHOD0(registerForAnimation, void());
  MOCK_METHOD0(scheduleAnimation, bool());

  bool userInputScrollable(ScrollbarOrientation) const override { return true; }
  bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  IntSize scrollOffsetInt() const override { return IntSize(); }
  int visibleHeight() const override { return 768; }
  int visibleWidth() const override { return 1024; }
  bool scrollAnimatorEnabled() const override {
    return m_scrollAnimatorEnabled;
  }
  int pageStep(ScrollbarOrientation) const override { return 0; }
  IntSize minimumScrollOffsetInt() const override {
    return flooredIntSize(m_minOffset);
  }
  IntSize maximumScrollOffsetInt() const override {
    return flooredIntSize(m_maxOffset);
  }

  void setScrollAnimator(ScrollAnimator* scrollAnimator) {
    animator = scrollAnimator;
  }

  ScrollOffset scrollOffset() const override {
    if (animator)
      return animator->currentOffset();
    return ScrollableArea::scrollOffset();
  }

  void setScrollOffset(const ScrollOffset& offset,
                       ScrollType type,
                       ScrollBehavior behavior = ScrollBehaviorInstant) {
    if (animator)
      animator->setCurrentOffset(offset);
    ScrollableArea::setScrollOffset(offset, type, behavior);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(animator);
    ScrollableArea::trace(visitor);
  }

 private:
  explicit MockScrollableArea(bool scrollAnimatorEnabled,
                              const ScrollOffset& minOffset,
                              const ScrollOffset& maxOffset)
      : m_scrollAnimatorEnabled(scrollAnimatorEnabled),
        m_minOffset(minOffset),
        m_maxOffset(maxOffset) {}

  bool m_scrollAnimatorEnabled;
  ScrollOffset m_minOffset;
  ScrollOffset m_maxOffset;
  Member<ScrollAnimator> animator;
};

class TestScrollAnimator : public ScrollAnimator {
 public:
  TestScrollAnimator(ScrollableArea* scrollableArea,
                     WTF::TimeFunction timingFunction)
      : ScrollAnimator(scrollableArea, timingFunction){};
  ~TestScrollAnimator() override{};

  void setShouldSendToCompositor(bool send) { m_shouldSendToCompositor = send; }

  bool sendAnimationToCompositor() override {
    if (m_shouldSendToCompositor) {
      m_runState =
          ScrollAnimatorCompositorCoordinator::RunState::RunningOnCompositor;
      m_compositorAnimationId = 1;
      return true;
    }
    return false;
  }

 protected:
  void abortAnimation() override {}

 private:
  bool m_shouldSendToCompositor = false;
};

}  // namespace

static void reset(ScrollAnimator& scrollAnimator) {
  scrollAnimator.scrollToOffsetWithoutAnimation(ScrollOffset());
}

// TODO(skobes): Add unit tests for composited scrolling paths.

TEST(ScrollAnimatorTest, MainThreadStates) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  ScrollAnimator* scrollAnimator =
      new ScrollAnimator(scrollableArea, getMockedTime);

  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(2);
  // Once from userScroll, once from updateCompositorAnimations.
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(2);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  // Idle
  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::Idle);

  // WaitingToSendToCompositor
  scrollAnimator->userScroll(ScrollByLine, FloatSize(10, 0));
  EXPECT_EQ(
      scrollAnimator->m_runState,
      ScrollAnimatorCompositorCoordinator::RunState::WaitingToSendToCompositor);

  // RunningOnMainThread
  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);
  scrollAnimator->tickAnimation(getMockedTime());
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);

  // PostAnimationCleanup
  scrollAnimator->cancelAnimation();
  EXPECT_EQ(
      scrollAnimator->m_runState,
      ScrollAnimatorCompositorCoordinator::RunState::PostAnimationCleanup);

  // Idle
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::Idle);

  reset(*scrollAnimator);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::current()->collectAllGarbage();
}

TEST(ScrollAnimatorTest, MainThreadEnabled) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  ScrollAnimator* scrollAnimator =
      new ScrollAnimator(scrollableArea, getMockedTime);

  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(9);
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(6);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

  ScrollResult result =
      scrollAnimator->userScroll(ScrollByLine, FloatSize(-100, 0));
  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_FALSE(result.didScrollX);
  EXPECT_FLOAT_EQ(-100.0f, result.unusedScrollDeltaX);

  result = scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_TRUE(result.didScrollX);
  EXPECT_FLOAT_EQ(0.0, result.unusedScrollDeltaX);

  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());

  EXPECT_NE(100, scrollAnimator->currentOffset().width());
  EXPECT_NE(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);

  scrollAnimator->userScroll(ScrollByPage, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());

  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());

  EXPECT_NE(100, scrollAnimator->currentOffset().width());
  EXPECT_NE(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);

  scrollAnimator->userScroll(ScrollByPixel, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());

  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());

  EXPECT_NE(100, scrollAnimator->currentOffset().width());
  EXPECT_NE(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());

  gMockedTime += 1.0;
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());

  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_EQ(100, scrollAnimator->currentOffset().width());

  reset(*scrollAnimator);

  scrollAnimator->userScroll(ScrollByPrecisePixel, FloatSize(100, 0));
  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

  EXPECT_EQ(100, scrollAnimator->currentOffset().width());
  EXPECT_NE(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);
}

// Test that a smooth scroll offset animation is aborted when followed by a
// non-smooth scroll offset animation.
TEST(ScrollAnimatorTest, AnimatedScrollAborted) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  ScrollAnimator* scrollAnimator =
      new ScrollAnimator(scrollableArea, getMockedTime);

  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(3);
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(2);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

  // Smooth scroll.
  ScrollResult result =
      scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_TRUE(result.didScrollX);
  EXPECT_FLOAT_EQ(0.0, result.unusedScrollDeltaX);
  EXPECT_TRUE(scrollAnimator->hasRunningAnimation());

  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());

  EXPECT_NE(100, scrollAnimator->currentOffset().width());
  EXPECT_NE(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());

  float x = scrollAnimator->currentOffset().width();

  // Instant scroll.
  result = scrollAnimator->userScroll(ScrollByPrecisePixel, FloatSize(100, 0));
  EXPECT_TRUE(result.didScrollX);
  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  EXPECT_FALSE(scrollAnimator->hasRunningAnimation());
  EXPECT_EQ(x + 100, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());

  reset(*scrollAnimator);
}

// Test that a smooth scroll offset animation running on the compositor is
// completed on the main thread.
TEST(ScrollAnimatorTest, AnimatedScrollTakeover) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  TestScrollAnimator* scrollAnimator =
      new TestScrollAnimator(scrollableArea, getMockedTime);

  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(2);
  // Called from userScroll, updateCompositorAnimations, then
  // takeOverCompositorAnimation (to re-register after RunningOnCompositor).
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(3);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

  // Smooth scroll.
  ScrollResult result =
      scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_TRUE(result.didScrollX);
  EXPECT_FLOAT_EQ(0.0, result.unusedScrollDeltaX);
  EXPECT_TRUE(scrollAnimator->hasRunningAnimation());

  // Update compositor animation.
  gMockedTime += 0.05;
  scrollAnimator->setShouldSendToCompositor(true);
  scrollAnimator->updateCompositorAnimations();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnCompositor);

  // Takeover.
  scrollAnimator->takeOverCompositorAnimation();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::
                RunningOnCompositorButNeedsTakeover);

  // Animation should now be running on the main thread.
  scrollAnimator->setShouldSendToCompositor(false);
  scrollAnimator->updateCompositorAnimations();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);
  scrollAnimator->tickAnimation(getMockedTime());
  EXPECT_NE(100, scrollAnimator->currentOffset().width());
  EXPECT_NE(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);
}

TEST(ScrollAnimatorTest, Disabled) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      false, ScrollOffset(), ScrollOffset(1000, 1000));
  ScrollAnimator* scrollAnimator =
      new ScrollAnimator(scrollableArea, getMockedTime);

  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(8);
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(0);

  scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_EQ(100, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);

  scrollAnimator->userScroll(ScrollByPage, FloatSize(100, 0));
  EXPECT_EQ(100, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);

  scrollAnimator->userScroll(ScrollByDocument, FloatSize(100, 0));
  EXPECT_EQ(100, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);

  scrollAnimator->userScroll(ScrollByPixel, FloatSize(100, 0));
  EXPECT_EQ(100, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);
}

// Test that cancelling an animation resets the animation state.
// See crbug.com/598548.
TEST(ScrollAnimatorTest, CancellingAnimationResetsState) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  ScrollAnimator* scrollAnimator =
      new ScrollAnimator(scrollableArea, getMockedTime);

  // Called from first userScroll, setCurrentOffset, and second userScroll.
  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(3);
  // Called from userScroll, updateCompositorAnimations.
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(4);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(0, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());

  // WaitingToSendToCompositor
  scrollAnimator->userScroll(ScrollByLine, FloatSize(10, 0));
  EXPECT_EQ(
      scrollAnimator->m_runState,
      ScrollAnimatorCompositorCoordinator::RunState::WaitingToSendToCompositor);

  // RunningOnMainThread
  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);
  scrollAnimator->tickAnimation(getMockedTime());
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnMainThread);

  // Amount scrolled so far.
  float offsetX = scrollAnimator->currentOffset().width();

  // Interrupt user scroll.
  scrollAnimator->cancelAnimation();
  EXPECT_EQ(
      scrollAnimator->m_runState,
      ScrollAnimatorCompositorCoordinator::RunState::PostAnimationCleanup);

  // Another userScroll after modified scroll offset.
  scrollAnimator->setCurrentOffset(ScrollOffset(offsetX + 15, 0));
  scrollAnimator->userScroll(ScrollByLine, FloatSize(10, 0));
  EXPECT_EQ(
      scrollAnimator->m_runState,
      ScrollAnimatorCompositorCoordinator::RunState::WaitingToSendToCompositor);

  // Finish scroll animation.
  gMockedTime += 1.0;
  scrollAnimator->updateCompositorAnimations();
  scrollAnimator->tickAnimation(getMockedTime());
  EXPECT_EQ(
      scrollAnimator->m_runState,
      ScrollAnimatorCompositorCoordinator::RunState::PostAnimationCleanup);

  EXPECT_EQ(offsetX + 15 + 10, scrollAnimator->currentOffset().width());
  EXPECT_EQ(0, scrollAnimator->currentOffset().height());
  reset(*scrollAnimator);
}

// Test the behavior when in WaitingToCancelOnCompositor and a new user scroll
// happens.
TEST(ScrollAnimatorTest, CancellingCompositorAnimation) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  TestScrollAnimator* scrollAnimator =
      new TestScrollAnimator(scrollableArea, getMockedTime);

  // Called when reset, not setting anywhere else.
  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(1);
  // Called from userScroll, and first update.
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(4);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scrollAnimator->hasAnimationThatRequiresService());

  // First user scroll.
  ScrollResult result =
      scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_TRUE(result.didScrollX);
  EXPECT_FLOAT_EQ(0.0, result.unusedScrollDeltaX);
  EXPECT_TRUE(scrollAnimator->hasRunningAnimation());
  EXPECT_EQ(100, scrollAnimator->desiredTargetOffset().width());
  EXPECT_EQ(0, scrollAnimator->desiredTargetOffset().height());

  // Update compositor animation.
  gMockedTime += 0.05;
  scrollAnimator->setShouldSendToCompositor(true);
  scrollAnimator->updateCompositorAnimations();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnCompositor);

  // Cancel
  scrollAnimator->cancelAnimation();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::
                WaitingToCancelOnCompositor);

  // Unrelated scroll offset update.
  scrollAnimator->setCurrentOffset(ScrollOffset(50, 0));

  // Desired target offset should be that of the second scroll.
  result = scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_TRUE(result.didScrollX);
  EXPECT_FLOAT_EQ(0.0, result.unusedScrollDeltaX);
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::
                WaitingToCancelOnCompositorButNewScroll);
  EXPECT_EQ(150, scrollAnimator->desiredTargetOffset().width());
  EXPECT_EQ(0, scrollAnimator->desiredTargetOffset().height());

  // Update compositor animation.
  gMockedTime += 0.05;
  scrollAnimator->updateCompositorAnimations();
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::RunningOnCompositor);

  // Third user scroll after compositor update updates the target.
  result = scrollAnimator->userScroll(ScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scrollAnimator->hasAnimationThatRequiresService());
  EXPECT_TRUE(result.didScrollX);
  EXPECT_FLOAT_EQ(0.0, result.unusedScrollDeltaX);
  EXPECT_EQ(scrollAnimator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::
                RunningOnCompositorButNeedsUpdate);
  EXPECT_EQ(250, scrollAnimator->desiredTargetOffset().width());
  EXPECT_EQ(0, scrollAnimator->desiredTargetOffset().height());
  reset(*scrollAnimator);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::current()->collectAllGarbage();
}

// This test verifies that impl only animation updates get cleared once they
// are pushed to compositor animation host.
TEST(ScrollAnimatorTest, ImplOnlyAnimationUpdatesCleared) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(), ScrollOffset(1000, 1000));
  TestScrollAnimator* animator =
      new TestScrollAnimator(scrollableArea, getMockedTime);

  // From calls to adjust/takeoverImplOnlyScrollOffsetAnimation.
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(3);

  // Verify that the adjustment update is cleared.
  EXPECT_EQ(animator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::Idle);
  EXPECT_FALSE(animator->hasAnimationThatRequiresService());
  EXPECT_TRUE(animator->implOnlyAnimationAdjustmentForTesting().isZero());

  animator->adjustImplOnlyScrollOffsetAnimation(IntSize(100, 100));
  animator->adjustImplOnlyScrollOffsetAnimation(IntSize(10, -10));

  EXPECT_TRUE(animator->hasAnimationThatRequiresService());
  EXPECT_EQ(FloatSize(110, 90),
            animator->implOnlyAnimationAdjustmentForTesting());

  animator->updateCompositorAnimations();

  EXPECT_EQ(animator->m_runState,
            ScrollAnimatorCompositorCoordinator::RunState::Idle);
  EXPECT_FALSE(animator->hasAnimationThatRequiresService());
  EXPECT_TRUE(animator->implOnlyAnimationAdjustmentForTesting().isZero());

  // Verify that the takeover update is cleared.
  animator->takeOverImplOnlyScrollOffsetAnimation();
  EXPECT_FALSE(animator->hasAnimationThatRequiresService());

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::current()->collectAllGarbage();
}

TEST(ScrollAnimatorTest, MainThreadAnimationTargetAdjustment) {
  MockScrollableArea* scrollableArea = MockScrollableArea::create(
      true, ScrollOffset(-100, -100), ScrollOffset(1000, 1000));
  ScrollAnimator* animator = new ScrollAnimator(scrollableArea, getMockedTime);
  scrollableArea->setScrollAnimator(animator);

  // Twice from tickAnimation, once from reset, and twice from
  // adjustAnimationAndSetScrollOffset.
  EXPECT_CALL(*scrollableArea, updateScrollOffset(_, _)).Times(5);
  // One from call to userScroll and one from updateCompositorAnimations.
  EXPECT_CALL(*scrollableArea, registerForAnimation()).Times(2);
  EXPECT_CALL(*scrollableArea, scheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  // Idle
  EXPECT_FALSE(animator->hasAnimationThatRequiresService());
  EXPECT_EQ(ScrollOffset(), animator->currentOffset());

  // WaitingToSendToCompositor
  animator->userScroll(ScrollByLine, ScrollOffset(100, 100));

  // RunningOnMainThread
  gMockedTime += 0.05;
  animator->updateCompositorAnimations();
  animator->tickAnimation(getMockedTime());
  ScrollOffset offset = animator->currentOffset();
  EXPECT_EQ(ScrollOffset(100, 100), animator->desiredTargetOffset());
  EXPECT_GT(offset.width(), 0);
  EXPECT_GT(offset.height(), 0);

  // Adjustment
  ScrollOffset newOffset = offset + ScrollOffset(10, -10);
  animator->adjustAnimationAndSetScrollOffset(newOffset, AnchoringScroll);
  EXPECT_EQ(ScrollOffset(110, 90), animator->desiredTargetOffset());

  // Adjusting after finished animation should do nothing.
  gMockedTime += 1.0;
  animator->updateCompositorAnimations();
  animator->tickAnimation(getMockedTime());
  EXPECT_EQ(
      animator->runStateForTesting(),
      ScrollAnimatorCompositorCoordinator::RunState::PostAnimationCleanup);
  newOffset = animator->currentOffset() + ScrollOffset(10, -10);
  animator->adjustAnimationAndSetScrollOffset(newOffset, AnchoringScroll);
  EXPECT_EQ(
      animator->runStateForTesting(),
      ScrollAnimatorCompositorCoordinator::RunState::PostAnimationCleanup);
  EXPECT_EQ(ScrollOffset(110, 90), animator->desiredTargetOffset());

  reset(*animator);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::current()->collectAllGarbage();
}

}  // namespace blink
