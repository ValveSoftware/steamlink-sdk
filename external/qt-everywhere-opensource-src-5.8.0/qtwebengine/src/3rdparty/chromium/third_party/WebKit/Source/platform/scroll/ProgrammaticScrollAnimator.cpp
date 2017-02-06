// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ProgrammaticScrollAnimator.h"

#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ProgrammaticScrollAnimator::ProgrammaticScrollAnimator(ScrollableArea* scrollableArea)
    : m_scrollableArea(scrollableArea)
    , m_startTime(0.0)
{
}

ProgrammaticScrollAnimator::~ProgrammaticScrollAnimator()
{
}

void ProgrammaticScrollAnimator::resetAnimationState()
{
    ScrollAnimatorCompositorCoordinator::resetAnimationState();
    m_animationCurve.reset();
    m_startTime = 0.0;
}

void ProgrammaticScrollAnimator::notifyPositionChanged(const DoublePoint& offset)
{
    m_scrollableArea->scrollPositionChanged(offset, ProgrammaticScroll);
}

void ProgrammaticScrollAnimator::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    cancelAnimation();
    notifyPositionChanged(offset);
}

void ProgrammaticScrollAnimator::animateToOffset(FloatPoint offset)
{
    if (m_runState == RunState::PostAnimationCleanup)
        resetAnimationState();

    m_startTime = 0.0;
    m_targetOffset = offset;
    m_animationCurve = CompositorScrollOffsetAnimationCurve::create(
        compositorOffsetFromBlinkOffset(m_targetOffset),
        CompositorScrollOffsetAnimationCurve::ScrollDurationDeltaBased);

    m_scrollableArea->registerForAnimation();
    if (!m_scrollableArea->scheduleAnimation()) {
        resetAnimationState();
        notifyPositionChanged(IntPoint(offset.x(), offset.y()));
    }
    m_runState = RunState::WaitingToSendToCompositor;
}

void ProgrammaticScrollAnimator::cancelAnimation()
{
    ASSERT(m_runState != RunState::RunningOnCompositorButNeedsUpdate);
    ScrollAnimatorCompositorCoordinator::cancelAnimation();
}

void ProgrammaticScrollAnimator::tickAnimation(double monotonicTime)
{
    if (m_runState != RunState::RunningOnMainThread)
        return;

    if (!m_startTime)
        m_startTime = monotonicTime;
    double elapsedTime = monotonicTime - m_startTime;
    bool isFinished = (elapsedTime > m_animationCurve->duration());
    FloatPoint offset = blinkOffsetFromCompositorOffset(m_animationCurve->getValue(elapsedTime));
    notifyPositionChanged(IntPoint(offset.x(), offset.y()));

    if (isFinished) {
        m_runState = RunState::PostAnimationCleanup;
    } else if (!m_scrollableArea->scheduleAnimation()) {
        notifyPositionChanged(IntPoint(m_targetOffset.x(), m_targetOffset.y()));
        resetAnimationState();
    }
}

void ProgrammaticScrollAnimator::updateCompositorAnimations()
{
    if (m_runState == RunState::PostAnimationCleanup) {
        // No special cleanup, simply reset animation state. We have this state
        // here because the state machine is shared with ScrollAnimator which
        // has to do some cleanup that requires the compositing state to be clean.
        return resetAnimationState();
    }

    if (m_compositorAnimationId && m_runState != RunState::RunningOnCompositor) {
        // If the current run state is WaitingToSendToCompositor but we have a
        // non-zero compositor animation id, there's a currently running
        // compositor animation that needs to be removed here before the new
        // animation is added below.
        ASSERT(m_runState == RunState::WaitingToCancelOnCompositor || m_runState == RunState::WaitingToSendToCompositor);

        removeAnimation();

        m_compositorAnimationId = 0;
        m_compositorAnimationGroupId = 0;
        if (m_runState == RunState::WaitingToCancelOnCompositor) {
            resetAnimationState();
            return;
        }
    }

    if (m_runState == RunState::WaitingToSendToCompositor) {
        if (!m_compositorAnimationAttachedToElementId)
            reattachCompositorPlayerIfNeeded(getScrollableArea()->compositorAnimationTimeline());

        bool sentToCompositor = false;

        if (!m_scrollableArea->shouldScrollOnMainThread()) {
            std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(*m_animationCurve, CompositorTargetProperty::SCROLL_OFFSET, 0, 0);

            int animationId = animation->id();
            int animationGroupId = animation->group();

            if (addAnimation(std::move(animation))) {
                sentToCompositor = true;
                m_runState = RunState::RunningOnCompositor;
                m_compositorAnimationId = animationId;
                m_compositorAnimationGroupId = animationGroupId;
            }
        }

        if (!sentToCompositor) {
            m_runState = RunState::RunningOnMainThread;
            m_animationCurve->setInitialValue(compositorOffsetFromBlinkOffset(
                FloatPoint(m_scrollableArea->scrollPosition())));
            if (!m_scrollableArea->scheduleAnimation()) {
                notifyPositionChanged(IntPoint(m_targetOffset.x(), m_targetOffset.y()));
                resetAnimationState();
            }
        }
    }
}

void ProgrammaticScrollAnimator::layerForCompositedScrollingDidChange(CompositorAnimationTimeline* timeline)
{
    reattachCompositorPlayerIfNeeded(timeline);

    // If the composited scrolling layer is lost during a composited animation,
    // continue the animation on the main thread.
    if (m_runState == RunState::RunningOnCompositor && !m_scrollableArea->layerForScrolling()) {
        m_runState = RunState::RunningOnMainThread;
        m_compositorAnimationId = 0;
        m_compositorAnimationGroupId = 0;
        m_animationCurve->setInitialValue(compositorOffsetFromBlinkOffset(
            FloatPoint(m_scrollableArea->scrollPosition())));
        m_scrollableArea->registerForAnimation();
        if (!m_scrollableArea->scheduleAnimation()) {
            resetAnimationState();
            notifyPositionChanged(IntPoint(m_targetOffset.x(), m_targetOffset.y()));
        }
    }
}

void ProgrammaticScrollAnimator::notifyCompositorAnimationFinished(int groupId)
{
    ASSERT(m_runState != RunState::RunningOnCompositorButNeedsUpdate);
    ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(groupId);
}

DEFINE_TRACE(ProgrammaticScrollAnimator)
{
    visitor->trace(m_scrollableArea);
    ScrollAnimatorCompositorCoordinator::trace(visitor);
}

} // namespace blink
