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

#ifndef ScrollAnimator_h
#define ScrollAnimator_h

#include "platform/Timer.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include <memory>

namespace blink {

class ScrollAnimatorTest;
class CompositorAnimationTimeline;

class PLATFORM_EXPORT ScrollAnimator : public ScrollAnimatorBase {
public:
    explicit ScrollAnimator(ScrollableArea*, WTF::TimeFunction = WTF::monotonicallyIncreasingTime);
    ~ScrollAnimator() override;

    bool hasRunningAnimation() const override;
    FloatSize computeDeltaToConsume(const FloatSize& delta) const override;

    ScrollResult userScroll(ScrollGranularity, const FloatSize& delta) override;
    void scrollToOffsetWithoutAnimation(const FloatPoint&) override;
    FloatPoint desiredTargetPosition() const override;

    // ScrollAnimatorCompositorCoordinator implementation.
    void tickAnimation(double monotonicTime) override;
    void cancelAnimation() override;
    void adjustAnimationAndSetScrollPosition(IntSize adjustment, ScrollType) override;
    void takeOverCompositorAnimation() override;
    void resetAnimationState() override;
    void updateCompositorAnimations() override;
    void notifyCompositorAnimationFinished(int groupId) override;
    void notifyCompositorAnimationAborted(int groupId) override;
    void layerForCompositedScrollingDidChange(CompositorAnimationTimeline*) override;

    DECLARE_VIRTUAL_TRACE();

protected:
    // Returns whether or not the animation was sent to the compositor.
    virtual bool sendAnimationToCompositor();

    void notifyAnimationTakeover(
        double monotonicTime,
        double animationStartTime,
        std::unique_ptr<cc::AnimationCurve>) override;

    std::unique_ptr<CompositorScrollOffsetAnimationCurve> m_animationCurve;
    double m_startTime;
    WTF::TimeFunction m_timeFunction;

private:
    // Returns true if the animation was scheduled successfully. If animation
    // could not be scheduled (e.g. because the frame is detached), scrolls
    // immediately to the target and returns false.
    bool registerAndScheduleAnimation();

    void createAnimationCurve();
    void postAnimationCleanupAndReset();

    void addMainThreadScrollingReason();
    void removeMainThreadScrollingReason();

    // Returns true if will animate to the given target position. Returns false
    // only when there is no animation running and we are not starting one
    // because we are already at targetPos.
    bool willAnimateToOffset(const FloatPoint& targetPos);

    FloatPoint m_targetOffset;
    ScrollGranularity m_lastGranularity;
};

} // namespace blink

#endif // ScrollAnimator_h
