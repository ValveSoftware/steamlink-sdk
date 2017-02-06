/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/scroll/ScrollAnimator.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "platform/TraceEvent.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/CurrentTime.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

WebLayer* toWebLayer(GraphicsLayer* layer)
{
    return layer ? layer->platformLayer() : nullptr;
}

} // namespace

ScrollAnimatorBase* ScrollAnimatorBase::create(ScrollableArea* scrollableArea)
{
    if (scrollableArea && scrollableArea->scrollAnimatorEnabled())
        return new ScrollAnimator(scrollableArea);
    return new ScrollAnimatorBase(scrollableArea);
}

ScrollAnimator::ScrollAnimator(ScrollableArea* scrollableArea, WTF::TimeFunction timeFunction)
    : ScrollAnimatorBase(scrollableArea)
    , m_timeFunction(timeFunction)
    , m_lastGranularity(ScrollByPixel)
{
}

ScrollAnimator::~ScrollAnimator()
{
}

FloatPoint ScrollAnimator::desiredTargetPosition() const
{
    if (m_runState == RunState::WaitingToCancelOnCompositor)
        return currentPosition();
    return (m_animationCurve || m_runState == RunState::WaitingToSendToCompositor)
        ? m_targetOffset : currentPosition();
}

bool ScrollAnimator::hasRunningAnimation() const
{
    return (m_animationCurve || m_runState == RunState::WaitingToSendToCompositor);
}

FloatSize ScrollAnimator::computeDeltaToConsume(const FloatSize& delta) const
{
    FloatPoint pos = desiredTargetPosition();
    FloatPoint newPos = toFloatPoint(m_scrollableArea->clampScrollPosition(pos + delta));
    return newPos - pos;
}

void ScrollAnimator::resetAnimationState()
{
    ScrollAnimatorCompositorCoordinator::resetAnimationState();
    if (m_animationCurve)
        m_animationCurve.reset();
    m_startTime = 0.0;
}

ScrollResult ScrollAnimator::userScroll(
    ScrollGranularity granularity, const FloatSize& delta)
{
    if (!m_scrollableArea->scrollAnimatorEnabled())
        return ScrollAnimatorBase::userScroll(granularity, delta);

    TRACE_EVENT0("blink", "ScrollAnimator::scroll");

    if (granularity == ScrollByPrecisePixel) {
        // Cancel scroll animation because asked to instant scroll.
        if (hasRunningAnimation())
            cancelAnimation();
        return ScrollAnimatorBase::userScroll(granularity, delta);
    }

    bool needsPostAnimationCleanup = m_runState == RunState::PostAnimationCleanup;
    if (m_runState == RunState::PostAnimationCleanup)
        resetAnimationState();

    FloatSize consumedDelta = computeDeltaToConsume(delta);
    FloatPoint targetPos = desiredTargetPosition();
    targetPos.move(consumedDelta);

    if (willAnimateToOffset(targetPos)) {
        m_lastGranularity = granularity;
        // Report unused delta only if there is no animation running. See
        // comment below regarding scroll latching.
        // TODO(bokan): Need to standardize how ScrollAnimators report
        // unusedDelta. This differs from ScrollAnimatorMac currently.
        return ScrollResult(true, true, 0, 0);
    }

    // If the run state when this method was called was PostAnimationCleanup and
    // we're not starting an animation, stay in PostAnimationCleanup state so
    // that the main thread scrolling reason can be removed.
    if (needsPostAnimationCleanup)
        m_runState = RunState::PostAnimationCleanup;

    // Report unused delta only if there is no animation and we are not
    // starting one. This ensures we latch for the duration of the
    // animation rather than animating multiple scrollers at the same time.
    return ScrollResult(false, false, delta.width(), delta.height());
}

bool ScrollAnimator::willAnimateToOffset(const FloatPoint& targetPos)
{
    if (m_runState == RunState::PostAnimationCleanup)
        resetAnimationState();

    if (m_runState == RunState::WaitingToCancelOnCompositor
        || m_runState == RunState::WaitingToCancelOnCompositorButNewScroll) {
        ASSERT(m_animationCurve);
        m_targetOffset = targetPos;
        if (registerAndScheduleAnimation())
            m_runState = RunState::WaitingToCancelOnCompositorButNewScroll;
        return true;
    }

    if (m_animationCurve) {
        if ((targetPos - m_targetOffset).isZero())
            return true;

        m_targetOffset = targetPos;
        ASSERT(m_runState == RunState::RunningOnMainThread
            || m_runState == RunState::RunningOnCompositor
            || m_runState == RunState::RunningOnCompositorButNeedsUpdate
            || m_runState == RunState::RunningOnCompositorButNeedsTakeover);

        // Running on the main thread, simply update the target offset instead
        // of sending to the compositor.
        if (m_runState == RunState::RunningOnMainThread) {
            m_animationCurve->updateTarget(m_timeFunction() - m_startTime,
                compositorOffsetFromBlinkOffset(targetPos));
            return true;
        }

        if (registerAndScheduleAnimation())
            m_runState = RunState::RunningOnCompositorButNeedsUpdate;
        return true;
    }

    if ((targetPos - currentPosition()).isZero())
        return false;

    m_targetOffset = targetPos;
    m_startTime = m_timeFunction();

    if (registerAndScheduleAnimation()) {
        if (m_scrollableArea->shouldScrollOnMainThread()) {
            createAnimationCurve();
            m_runState = RunState::RunningOnMainThread;
        } else {
            m_runState = RunState::WaitingToSendToCompositor;
        }
    }

    return true;
}

void ScrollAnimator::adjustAnimationAndSetScrollPosition(
    IntSize adjustment, ScrollType scrollType)
{
    DoublePoint adjustedPos = m_scrollableArea->clampScrollPosition(
        m_scrollableArea->scrollPositionDouble() + adjustment);
    IntSize actualAdjustment = roundedIntPoint(adjustedPos) -
        roundedIntPoint(m_scrollableArea->scrollPositionDouble());

    m_scrollableArea->setScrollPosition(adjustedPos, scrollType);

    if (m_runState == RunState::Idle) {
        adjustImplOnlyScrollOffsetAnimation(actualAdjustment);
    } else if (hasRunningAnimation()) {
        m_targetOffset += toFloatSize(actualAdjustment);
        if (m_animationCurve) {
            m_animationCurve->applyAdjustment(actualAdjustment);
            if (m_runState != RunState::RunningOnMainThread && registerAndScheduleAnimation())
                m_runState = RunState::RunningOnCompositorButNeedsAdjustment;
        }
    }
}

void ScrollAnimator::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    m_currentPos = offset;

    resetAnimationState();
    notifyPositionChanged();
}

void ScrollAnimator::tickAnimation(double monotonicTime)
{
    if (m_runState != RunState::RunningOnMainThread)
        return;

    TRACE_EVENT0("blink", "ScrollAnimator::tickAnimation");
    double elapsedTime = monotonicTime - m_startTime;

    bool isFinished = (elapsedTime > m_animationCurve->duration());
    FloatPoint offset = blinkOffsetFromCompositorOffset(isFinished
        ? m_animationCurve->targetValue()
        : m_animationCurve->getValue(elapsedTime));

    offset = FloatPoint(m_scrollableArea->clampScrollPosition(offset));

    m_currentPos = offset;

    if (isFinished)
        m_runState = RunState::PostAnimationCleanup;
    else
        getScrollableArea()->scheduleAnimation();

    TRACE_EVENT0("blink", "ScrollAnimator::notifyPositionChanged");
    notifyPositionChanged();
}

void ScrollAnimator::postAnimationCleanupAndReset()
{
    // Remove the temporary main thread scrolling reason that was added while
    // main thread had scheduled an animation.
    removeMainThreadScrollingReason();

    resetAnimationState();
}

bool ScrollAnimator::sendAnimationToCompositor()
{
    if (m_scrollableArea->shouldScrollOnMainThread())
        return false;

    std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(*m_animationCurve, CompositorTargetProperty::SCROLL_OFFSET, 0, 0);
    // Being here means that either there is an animation that needs
    // to be sent to the compositor, or an animation that needs to
    // be updated (a new scroll event before the previous animation
    // is finished). In either case, the start time is when the
    // first animation was initiated. This re-targets the animation
    // using the current time on main thread.
    animation->setStartTime(m_startTime);

    int animationId = animation->id();
    int animationGroupId = animation->group();

    bool sentToCompositor = addAnimation(std::move(animation));
    if (sentToCompositor) {
        m_runState = RunState::RunningOnCompositor;
        m_compositorAnimationId = animationId;
        m_compositorAnimationGroupId = animationGroupId;
    }

    return sentToCompositor;
}

void ScrollAnimator::createAnimationCurve()
{
    DCHECK(!m_animationCurve);
    m_animationCurve = CompositorScrollOffsetAnimationCurve::create(
        compositorOffsetFromBlinkOffset(m_targetOffset),
        m_lastGranularity == ScrollByPixel ?
            CompositorScrollOffsetAnimationCurve::ScrollDurationInverseDelta :
            CompositorScrollOffsetAnimationCurve::ScrollDurationConstant);
    m_animationCurve->setInitialValue(compositorOffsetFromBlinkOffset(currentPosition()));
}

void ScrollAnimator::updateCompositorAnimations()
{
    ScrollAnimatorCompositorCoordinator::updateCompositorAnimations();
    if (m_runState == RunState::RunningOnMainThread) {
        // We add a temporary main thread scrolling reason so that subsequent
        // scrolls get handled on the main thread. This is removed when the
        // animation is finished in ::tickAnimation.
        addMainThreadScrollingReason();
        return;
    }

    if (m_runState == RunState::PostAnimationCleanup) {
        postAnimationCleanupAndReset();
        return;
    }

    if (m_runState == RunState::WaitingToCancelOnCompositor) {
        DCHECK(m_compositorAnimationId);
        abortAnimation();
        postAnimationCleanupAndReset();
        return;
    }

    if (m_runState == RunState::RunningOnCompositorButNeedsTakeover) {
        // The call to ::takeOverCompositorAnimation aborted the animation and
        // put us in this state. The assumption is that takeOver is called
        // because a main thread scrolling reason is added, and simply trying
        // to ::sendAnimationToCompositor will fail and we will run on the main
        // thread.
        resetAnimationIds();
        m_runState = RunState::WaitingToSendToCompositor;
    }

    if (m_runState == RunState::RunningOnCompositorButNeedsUpdate
        || m_runState == RunState::WaitingToCancelOnCompositorButNewScroll
        || m_runState == RunState::RunningOnCompositorButNeedsAdjustment) {
        // Abort the running animation before a new one with an updated
        // target is added.
        abortAnimation();
        resetAnimationIds();

        if (m_runState != RunState::RunningOnCompositorButNeedsAdjustment) {
            // When in RunningOnCompositorButNeedsAdjustment, the call to
            // ::adjustScrollOffsetAnimation should have made the necessary
            // adjustment to the curve.
            m_animationCurve->updateTarget(m_timeFunction() - m_startTime,
                compositorOffsetFromBlinkOffset(m_targetOffset));
        }

        if (m_runState == RunState::WaitingToCancelOnCompositorButNewScroll)
            m_animationCurve->setInitialValue(compositorOffsetFromBlinkOffset(currentPosition()));

        m_runState = RunState::WaitingToSendToCompositor;
    }

    if (m_runState == RunState::WaitingToSendToCompositor) {
        if (!m_compositorAnimationAttachedToElementId)
            reattachCompositorPlayerIfNeeded(getScrollableArea()->compositorAnimationTimeline());

        if (!m_animationCurve)
            createAnimationCurve();

        bool runningOnMainThread = false;
        bool sentToCompositor = sendAnimationToCompositor();
        if (!sentToCompositor) {
            runningOnMainThread = registerAndScheduleAnimation();
            if (runningOnMainThread)
                m_runState = RunState::RunningOnMainThread;
        }

        // Main thread should deal with the scroll animations it started.
        if (sentToCompositor || runningOnMainThread)
            addMainThreadScrollingReason();
        else
            removeMainThreadScrollingReason();
    }
}

void ScrollAnimator::addMainThreadScrollingReason()
{
    if (WebLayer* scrollLayer = toWebLayer(getScrollableArea()->layerForScrolling())) {
        scrollLayer->addMainThreadScrollingReasons(
            MainThreadScrollingReason::kAnimatingScrollOnMainThread);
    }
}

void ScrollAnimator::removeMainThreadScrollingReason()
{
    if (WebLayer* scrollLayer = toWebLayer(getScrollableArea()->layerForScrolling())) {
        scrollLayer->clearMainThreadScrollingReasons(
            MainThreadScrollingReason::kAnimatingScrollOnMainThread);
    }
}

void ScrollAnimator::notifyCompositorAnimationAborted(int groupId)
{
    // An animation aborted by the compositor is treated as a finished
    // animation.
    ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(groupId);
}

void ScrollAnimator::notifyCompositorAnimationFinished(int groupId)
{
    ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(groupId);
}

void ScrollAnimator::notifyAnimationTakeover(
    double monotonicTime,
    double animationStartTime,
    std::unique_ptr<cc::AnimationCurve> curve)
{
    // If there is already an animation running and the compositor asks to take
    // over an animation, do nothing to avoid judder.
    if (hasRunningAnimation())
        return;

    cc::ScrollOffsetAnimationCurve* scrollOffsetAnimationCurve =
        curve->ToScrollOffsetAnimationCurve();
    FloatPoint targetValue(scrollOffsetAnimationCurve->target_value().x(),
        scrollOffsetAnimationCurve->target_value().y());
    if (willAnimateToOffset(targetValue)) {
        m_animationCurve = CompositorScrollOffsetAnimationCurve::create(std::move(scrollOffsetAnimationCurve));
        m_startTime = animationStartTime;
    }
}

void ScrollAnimator::cancelAnimation()
{
    ScrollAnimatorCompositorCoordinator::cancelAnimation();
}

void ScrollAnimator::takeOverCompositorAnimation()
{
    if (m_runState == RunState::RunningOnCompositor
        || m_runState ==  RunState::RunningOnCompositorButNeedsUpdate)
        removeMainThreadScrollingReason();

    ScrollAnimatorCompositorCoordinator::takeOverCompositorAnimation();
}

void ScrollAnimator::layerForCompositedScrollingDidChange(
    CompositorAnimationTimeline* timeline)
{
    if (reattachCompositorPlayerIfNeeded(timeline) && m_animationCurve)
        addMainThreadScrollingReason();
}

bool ScrollAnimator::registerAndScheduleAnimation()
{
    getScrollableArea()->registerForAnimation();
    if (!m_scrollableArea->scheduleAnimation()) {
        scrollToOffsetWithoutAnimation(m_targetOffset);
        resetAnimationState();
        return false;
    }
    return true;
}

DEFINE_TRACE(ScrollAnimator)
{
    ScrollAnimatorBase::trace(visitor);
}

} // namespace blink
