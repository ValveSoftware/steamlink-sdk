// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ScrollAnimatorCompositorCoordinator::ScrollAnimatorCompositorCoordinator()
    : m_compositorAnimationAttachedToElementId()
    , m_runState(RunState::Idle)
    , m_compositorAnimationId(0)
    , m_compositorAnimationGroupId(0)
    , m_implOnlyAnimationTakeover(false)
{
    ThreadState::current()->registerPreFinalizer(this);
    m_compositorPlayer = CompositorAnimationPlayer::create();
    ASSERT(m_compositorPlayer);
    m_compositorPlayer->setAnimationDelegate(this);
}

ScrollAnimatorCompositorCoordinator::~ScrollAnimatorCompositorCoordinator()
{
}

void ScrollAnimatorCompositorCoordinator::dispose()
{
    m_compositorPlayer->setAnimationDelegate(nullptr);
    m_compositorPlayer.reset();
}

void ScrollAnimatorCompositorCoordinator::resetAnimationIds()
{
    m_compositorAnimationId = 0;
    m_compositorAnimationGroupId = 0;
}

void ScrollAnimatorCompositorCoordinator::resetAnimationState()
{
    m_runState = RunState::Idle;
    resetAnimationIds();
}

bool ScrollAnimatorCompositorCoordinator::hasAnimationThatRequiresService() const
{
    if (hasImplOnlyAnimationUpdate())
        return true;

    switch (m_runState) {
    case RunState::Idle:
    case RunState::RunningOnCompositor:
        return false;
    case RunState::WaitingToCancelOnCompositorButNewScroll:
    case RunState::PostAnimationCleanup:
    case RunState::WaitingToSendToCompositor:
    case RunState::RunningOnMainThread:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::RunningOnCompositorButNeedsAdjustment:
    case RunState::WaitingToCancelOnCompositor:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorCompositorCoordinator::addAnimation(
    std::unique_ptr<CompositorAnimation> animation)
{
    if (m_compositorPlayer->isElementAttached()) {
        m_compositorPlayer->addAnimation(animation.release());
        return true;
    }
    return false;
}

void ScrollAnimatorCompositorCoordinator::removeAnimation()
{
    if (m_compositorPlayer->isElementAttached())
        m_compositorPlayer->removeAnimation(m_compositorAnimationId);
}

void ScrollAnimatorCompositorCoordinator::abortAnimation()
{
    if (m_compositorPlayer->isElementAttached())
        m_compositorPlayer->abortAnimation(m_compositorAnimationId);
}

void ScrollAnimatorCompositorCoordinator::cancelAnimation()
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::WaitingToCancelOnCompositor:
    case RunState::PostAnimationCleanup:
        break;
    case RunState::WaitingToSendToCompositor:
        if (m_compositorAnimationId) {
            // We still have a previous animation running on the compositor.
            m_runState = RunState::WaitingToCancelOnCompositor;
        } else {
            resetAnimationState();
        }
        break;
    case RunState::RunningOnMainThread:
        m_runState = RunState::PostAnimationCleanup;
        break;
    case RunState::WaitingToCancelOnCompositorButNewScroll:
    case RunState::RunningOnCompositorButNeedsAdjustment:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositor:
        m_runState = RunState::WaitingToCancelOnCompositor;

        // Get serviced the next time compositor updates are allowed.
        getScrollableArea()->registerForAnimation();
    }
}

void ScrollAnimatorCompositorCoordinator::takeOverCompositorAnimation()
{
    switch (m_runState) {
    case RunState::Idle:
        takeOverImplOnlyScrollOffsetAnimation();
        break;
    case RunState::WaitingToCancelOnCompositor:
    case RunState::WaitingToCancelOnCompositorButNewScroll:
    case RunState::PostAnimationCleanup:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::WaitingToSendToCompositor:
    case RunState::RunningOnMainThread:
        break;
    case RunState::RunningOnCompositorButNeedsAdjustment:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositor:
        // We call abortAnimation that makes changes to the animation running on
        // the compositor. Thus, this function should only be called when in
        // CompositingClean state.
        abortAnimation();

        m_runState = RunState::RunningOnCompositorButNeedsTakeover;

        // Get serviced the next time compositor updates are allowed.
        getScrollableArea()->registerForAnimation();
    }
}

void ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(
    int groupId)
{
    if (m_compositorAnimationGroupId != groupId)
        return;

    m_compositorAnimationId = 0;
    m_compositorAnimationGroupId = 0;

    switch (m_runState) {
    case RunState::Idle:
    case RunState::PostAnimationCleanup:
    case RunState::RunningOnMainThread:
        ASSERT_NOT_REACHED();
        break;
    case RunState::WaitingToSendToCompositor:
    case RunState::WaitingToCancelOnCompositorButNewScroll:
        break;
    case RunState::RunningOnCompositor:
    case RunState::RunningOnCompositorButNeedsAdjustment:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::WaitingToCancelOnCompositor:
        m_runState = RunState::PostAnimationCleanup;
        // Get serviced the next time compositor updates are allowed.
        if (getScrollableArea())
            getScrollableArea()->registerForAnimation();
        else
            resetAnimationState();
    }
}

bool ScrollAnimatorCompositorCoordinator::reattachCompositorPlayerIfNeeded(
    CompositorAnimationTimeline* timeline)
{
    bool reattached = false;
    CompositorElementId compositorAnimationAttachedToElementId;
    if (getScrollableArea()->layerForScrolling()) {
        compositorAnimationAttachedToElementId = getScrollableArea()->layerForScrolling()->platformLayer()->elementId();
        DCHECK(compositorAnimationAttachedToElementId);
    }

    if (compositorAnimationAttachedToElementId != m_compositorAnimationAttachedToElementId) {
        if (m_compositorPlayer && timeline) {
            // Detach from old layer (if any).
            if (m_compositorAnimationAttachedToElementId) {
                if (m_compositorPlayer->isElementAttached())
                    m_compositorPlayer->detachElement();
                timeline->playerDestroyed(*this);
            }
            // Attach to new layer (if any).
            if (compositorAnimationAttachedToElementId) {
                DCHECK(!m_compositorPlayer->isElementAttached());
                timeline->playerAttached(*this);
                m_compositorPlayer->attachElement(compositorAnimationAttachedToElementId);
                reattached = true;
            }
            m_compositorAnimationAttachedToElementId = compositorAnimationAttachedToElementId;
        }
    }

    return reattached;
}

void ScrollAnimatorCompositorCoordinator::notifyAnimationStarted(
    double monotonicTime, int group)
{
}

void ScrollAnimatorCompositorCoordinator::notifyAnimationFinished(
    double monotonicTime, int group)
{
    notifyCompositorAnimationFinished(group);
}

void ScrollAnimatorCompositorCoordinator::notifyAnimationAborted(
    double monotonicTime, int group)
{
    // An animation aborted by the compositor is treated as a finished
    // animation.
    notifyCompositorAnimationFinished(group);
}

CompositorAnimationPlayer* ScrollAnimatorCompositorCoordinator::compositorPlayer() const
{
    return m_compositorPlayer.get();
}

FloatPoint ScrollAnimatorCompositorCoordinator::compositorOffsetFromBlinkOffset(FloatPoint offset)
{
    offset.moveBy(getScrollableArea()->scrollOrigin());
    return offset;
}

FloatPoint ScrollAnimatorCompositorCoordinator::blinkOffsetFromCompositorOffset(FloatPoint offset)
{
    offset.moveBy(-getScrollableArea()->scrollOrigin());
    return offset;
}

bool ScrollAnimatorCompositorCoordinator::hasImplOnlyAnimationUpdate() const
{
    return !m_implOnlyAnimationAdjustment.isZero() || m_implOnlyAnimationTakeover;
}

void ScrollAnimatorCompositorCoordinator::updateImplOnlyCompositorAnimations()
{
    if (!hasImplOnlyAnimationUpdate())
        return;

    GraphicsLayer* layer = getScrollableArea()->layerForScrolling();
    CompositorAnimationTimeline* timeline = getScrollableArea()->compositorAnimationTimeline();
    if (layer && timeline && !timeline->compositorAnimationHost().isNull()) {
        CompositorAnimationHost host = timeline->compositorAnimationHost();
        cc::ElementId elementId =layer->platformLayer()->elementId();
        if (!m_implOnlyAnimationAdjustment.isZero()) {
            host.adjustImplOnlyScrollOffsetAnimation(
                elementId,
                gfx::Vector2dF(m_implOnlyAnimationAdjustment.width(), m_implOnlyAnimationAdjustment.height()));
        }
        if (m_implOnlyAnimationTakeover)
            host.takeOverImplOnlyScrollOffsetAnimation(elementId);
    }
    m_implOnlyAnimationAdjustment = IntSize();
    m_implOnlyAnimationTakeover = false;
}

void ScrollAnimatorCompositorCoordinator::updateCompositorAnimations()
{
    if (!getScrollableArea()->scrollAnimatorEnabled())
        return;

    updateImplOnlyCompositorAnimations();
}

void ScrollAnimatorCompositorCoordinator::adjustAnimationAndSetScrollPosition(
    IntSize adjustment, ScrollType scrollType) {
    // Subclasses should override this and adjust the animation as necessary.
    getScrollableArea()->setScrollPosition(
        getScrollableArea()->scrollPositionDouble() + adjustment, scrollType);
}

void ScrollAnimatorCompositorCoordinator::adjustImplOnlyScrollOffsetAnimation(
    const IntSize& adjustment)
{
    if (!getScrollableArea()->scrollAnimatorEnabled())
        return;

    m_implOnlyAnimationAdjustment.expand(adjustment.width(), adjustment.height());

    getScrollableArea()->registerForAnimation();
}

void ScrollAnimatorCompositorCoordinator::takeOverImplOnlyScrollOffsetAnimation()
{
    if (!getScrollableArea()->scrollAnimatorEnabled())
        return;

    m_implOnlyAnimationTakeover = true;

    // Update compositor animations right away to avoid skipping a frame.
    // This imposes the constraint that this function should only be called
    // from or after DocumentLifecycle::LifecycleState::CompositingClean state.
    updateImplOnlyCompositorAnimations();

    getScrollableArea()->registerForAnimation();
}

String ScrollAnimatorCompositorCoordinator::runStateAsText() const
{
    switch (m_runState) {
    case RunState::Idle:
        return String("Idle");
    case RunState::WaitingToSendToCompositor:
        return String("WaitingToSendToCompositor");
    case RunState::RunningOnCompositor:
        return String("RunningOnCompositor");
    case RunState::RunningOnMainThread:
        return String("RunningOnMainThread");
    case RunState::RunningOnCompositorButNeedsUpdate:
        return String("RunningOnCompositorButNeedsUpdate");
    case RunState::WaitingToCancelOnCompositor:
        return String("WaitingToCancelOnCompositor");
    case RunState::PostAnimationCleanup:
        return String("PostAnimationCleanup");
    case RunState::RunningOnCompositorButNeedsTakeover:
        return String("RunningOnCompositorButNeedsTakeover");
    case RunState::WaitingToCancelOnCompositorButNewScroll:
        return String("WaitingToCancelOnCompositorButNewScroll");
    case RunState::RunningOnCompositorButNeedsAdjustment:
        return String("RunningOnCompositorButNeedsAdjustment");
    }
    ASSERT_NOT_REACHED();
    return String();
}

} // namespace blink
