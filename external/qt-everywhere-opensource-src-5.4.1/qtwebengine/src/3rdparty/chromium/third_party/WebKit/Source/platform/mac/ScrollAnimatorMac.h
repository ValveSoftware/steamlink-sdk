/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollAnimatorMac_h
#define ScrollAnimatorMac_h

#include "platform/Timer.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/mac/ScrollElasticityController.h"
#include "platform/scroll/ScrollAnimator.h"
#include "wtf/RetainPtr.h"

OBJC_CLASS WebScrollAnimationHelperDelegate;
OBJC_CLASS WebScrollbarPainterControllerDelegate;
OBJC_CLASS WebScrollbarPainterDelegate;

typedef id ScrollbarPainterController;

#if !USE(RUBBER_BANDING)
class ScrollElasticityControllerClient { };
#endif

namespace WebCore {

class Scrollbar;

class PLATFORM_EXPORT ScrollAnimatorMac : public ScrollAnimator, private ScrollElasticityControllerClient {

public:
    ScrollAnimatorMac(ScrollableArea*);
    virtual ~ScrollAnimatorMac();

    void immediateScrollToPointForScrollAnimation(const FloatPoint& newPosition);
    bool haveScrolledSincePageLoad() const { return m_haveScrolledSincePageLoad; }

    void updateScrollerStyle();

    bool scrollbarPaintTimerIsActive() const;
    void startScrollbarPaintTimer();
    void stopScrollbarPaintTimer();

    void sendContentAreaScrolledSoon(const FloatSize& scrollDelta);

    void setVisibleScrollerThumbRect(const IntRect&);

    static bool canUseCoordinatedScrollbar();

private:
    RetainPtr<id> m_scrollAnimationHelper;
    RetainPtr<WebScrollAnimationHelperDelegate> m_scrollAnimationHelperDelegate;

    RetainPtr<ScrollbarPainterController> m_scrollbarPainterController;
    RetainPtr<WebScrollbarPainterControllerDelegate> m_scrollbarPainterControllerDelegate;
    RetainPtr<WebScrollbarPainterDelegate> m_horizontalScrollbarPainterDelegate;
    RetainPtr<WebScrollbarPainterDelegate> m_verticalScrollbarPainterDelegate;

    void initialScrollbarPaintTimerFired(Timer<ScrollAnimatorMac>*);
    Timer<ScrollAnimatorMac> m_initialScrollbarPaintTimer;

    void sendContentAreaScrolledTimerFired(Timer<ScrollAnimatorMac>*);
    Timer<ScrollAnimatorMac> m_sendContentAreaScrolledTimer;
    FloatSize m_contentAreaScrolledTimerScrollDelta;

    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float delta) OVERRIDE;
    virtual void scrollToOffsetWithoutAnimation(const FloatPoint&) OVERRIDE;

#if USE(RUBBER_BANDING)
    virtual bool handleWheelEvent(const PlatformWheelEvent&) OVERRIDE;
#endif

    virtual void handleWheelEventPhase(PlatformWheelEventPhase) OVERRIDE;

    virtual void cancelAnimations() OVERRIDE;
    virtual void setIsActive() OVERRIDE;

    virtual void contentAreaWillPaint() const OVERRIDE;
    virtual void mouseEnteredContentArea() const OVERRIDE;
    virtual void mouseExitedContentArea() const OVERRIDE;
    virtual void mouseMovedInContentArea() const OVERRIDE;
    virtual void mouseEnteredScrollbar(Scrollbar*) const OVERRIDE;
    virtual void mouseExitedScrollbar(Scrollbar*) const OVERRIDE;
    virtual void willStartLiveResize() OVERRIDE;
    virtual void contentsResized() const OVERRIDE;
    virtual void willEndLiveResize() OVERRIDE;
    virtual void contentAreaDidShow() const OVERRIDE;
    virtual void contentAreaDidHide() const OVERRIDE;
    void didBeginScrollGesture() const;
    void didEndScrollGesture() const;
    void mayBeginScrollGesture() const;

    virtual void finishCurrentScrollAnimations() OVERRIDE;

    virtual void didAddVerticalScrollbar(Scrollbar*) OVERRIDE;
    virtual void willRemoveVerticalScrollbar(Scrollbar*) OVERRIDE;
    virtual void didAddHorizontalScrollbar(Scrollbar*) OVERRIDE;
    virtual void willRemoveHorizontalScrollbar(Scrollbar*) OVERRIDE;

    virtual bool shouldScrollbarParticipateInHitTesting(Scrollbar*) OVERRIDE;

    virtual void notifyContentAreaScrolled(const FloatSize& delta) OVERRIDE;

    FloatPoint adjustScrollPositionIfNecessary(const FloatPoint&) const;

    void immediateScrollTo(const FloatPoint&);

    virtual bool isRubberBandInProgress() const OVERRIDE;

#if USE(RUBBER_BANDING)
    /// ScrollElasticityControllerClient member functions.
    virtual IntSize stretchAmount() OVERRIDE;
    virtual bool allowsHorizontalStretching() OVERRIDE;
    virtual bool allowsVerticalStretching() OVERRIDE;
    virtual bool pinnedInDirection(const FloatSize&) OVERRIDE;
    virtual bool canScrollHorizontally() OVERRIDE;
    virtual bool canScrollVertically() OVERRIDE;
    virtual WebCore::IntPoint absoluteScrollPosition() OVERRIDE;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) OVERRIDE;
    virtual void immediateScrollBy(const FloatSize&) OVERRIDE;
    virtual void startSnapRubberbandTimer() OVERRIDE;
    virtual void stopSnapRubberbandTimer() OVERRIDE;
    virtual void adjustScrollPositionToBoundsIfNecessary() OVERRIDE;

    bool pinnedInDirection(float deltaX, float deltaY);
    void snapRubberBandTimerFired(Timer<ScrollAnimatorMac>*);

    ScrollElasticityController m_scrollElasticityController;
    Timer<ScrollAnimatorMac> m_snapRubberBandTimer;
#endif

    bool m_haveScrolledSincePageLoad;
    bool m_needsScrollerStyleUpdate;
    IntRect m_visibleScrollerThumbRect;
};

} // namespace WebCore

#endif // ScrollAnimatorMac_h
