/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef ScrollElasticityController_h
#define ScrollElasticityController_h

#if USE(RUBBER_BANDING)

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class PlatformWheelEvent;

class ScrollElasticityControllerClient {
protected:
    virtual ~ScrollElasticityControllerClient() { }

public:
    virtual bool allowsHorizontalStretching() = 0;
    virtual bool allowsVerticalStretching() = 0;
    // The amount that the view is stretched past the normal allowable bounds.
    // The "overhang" amount.
    virtual IntSize stretchAmount() = 0;
    virtual bool pinnedInDirection(const FloatSize&) = 0;
    virtual bool canScrollHorizontally() = 0;
    virtual bool canScrollVertically() = 0;

    // Return the absolute scroll position, not relative to the scroll origin.
    virtual WebCore::IntPoint absoluteScrollPosition() = 0;

    virtual void immediateScrollBy(const FloatSize&) = 0;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) = 0;
    virtual void startSnapRubberbandTimer() = 0;
    virtual void stopSnapRubberbandTimer() = 0;

    // If the current scroll position is within the overhang area, this function will cause
    // the page to scroll to the nearest boundary point.
    virtual void adjustScrollPositionToBoundsIfNecessary() = 0;
};

class PLATFORM_EXPORT ScrollElasticityController {
    WTF_MAKE_NONCOPYABLE(ScrollElasticityController);

public:
    explicit ScrollElasticityController(ScrollElasticityControllerClient*);

    // This method is responsible for both scrolling and rubber-banding.
    //
    // Events are passed by IPC from the embedder. Events on Mac are grouped
    // into "gestures". If this method returns 'true', then this object has
    // handled the event. It expects the embedder to continue to forward events
    // from the gesture.
    //
    // This method makes the assumption that there is only 1 input device being
    // used at a time. If the user simultaneously uses multiple input devices,
    // Cocoa does not correctly pass all the gestureBegin/End events. The state
    // of this class is guaranteed to become eventually consistent, once the
    // user stops using multiple input devices.
    bool handleWheelEvent(const PlatformWheelEvent&);
    void snapRubberBandTimerFired();

    bool isRubberBandInProgress() const;

private:
    void stopSnapRubberbandTimer();
    void snapRubberBand();

    // This method determines whether a given event should be handled. The
    // logic for control events of gestures (PhaseBegan, PhaseEnded) is handled
    // elsewhere.
    //
    // This class handles almost all wheel events. All of the following
    // conditions must be met for this class to ignore an event:
    // + No previous events in this gesture have caused any scrolling or rubber
    // banding.
    // + The event contains a horizontal component.
    // + The client's view is pinned in the horizontal direction of the event.
    // + The wheel event disallows rubber banding in the horizontal direction
    // of the event.
    bool shouldHandleEvent(const PlatformWheelEvent&);

    ScrollElasticityControllerClient* m_client;

    // There is an active scroll gesture event. This parameter only gets set to
    // false after the rubber band has been snapped, and before a new gesture
    // has begun. A careful audit of the code may deprecate the need for this
    // parameter.
    bool m_inScrollGesture;
    // At least one event in the current gesture has been consumed and has
    // caused the view to scroll or rubber band. All future events in this
    // gesture will be consumed and overscrolls will cause rubberbanding.
    bool m_hasScrolled;
    bool m_momentumScrollInProgress;
    bool m_ignoreMomentumScrolls;

    CFTimeInterval m_lastMomentumScrollTimestamp;
    FloatSize m_overflowScrollDelta;
    FloatSize m_stretchScrollForce;
    FloatSize m_momentumVelocity;

    // Rubber band state.
    CFTimeInterval m_startTime;
    FloatSize m_startStretch;
    FloatPoint m_origOrigin;
    FloatSize m_origVelocity;

    bool m_snapRubberbandTimerIsActive;
};

} // namespace WebCore

#endif // USE(RUBBER_BANDING)

#endif // ScrollElasticityController_h
