/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformGestureEvent_h
#define PlatformGestureEvent_h

#include "platform/PlatformEvent.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntSize.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Assertions.h"
#include <string.h>

namespace blink {

enum PlatformGestureSource {
    PlatformGestureSourceUninitialized,
    PlatformGestureSourceTouchpad,
    PlatformGestureSourceTouchscreen
};

class PlatformGestureEvent : public PlatformEvent {
public:
    PlatformGestureEvent()
        : PlatformEvent(PlatformEvent::GestureScrollBegin)
        , m_source(PlatformGestureSourceUninitialized)
    {
        memset(&m_data, 0, sizeof(m_data));
    }

    PlatformGestureEvent(EventType type, const IntPoint& position,
        const IntPoint& globalPosition, const IntSize& area, double timestamp,
        PlatformEvent::Modifiers modifiers, PlatformGestureSource source)
        : PlatformEvent(type, modifiers, timestamp)
        , m_position(position)
        , m_globalPosition(globalPosition)
        , m_area(area)
        , m_source(source)
    {
        memset(&m_data, 0, sizeof(m_data));
    }

    void setScrollGestureData(float deltaX, float deltaY, ScrollGranularity deltaUnits, float velocityX, float velocityY,
        ScrollInertialPhase inertialPhase, bool preventPropagation, int resendingPluginId)
    {
        ASSERT(type() == PlatformEvent::GestureScrollBegin
            || type() == PlatformEvent::GestureScrollUpdate
            || type() == PlatformEvent::GestureScrollEnd);
        if (type() != GestureScrollUpdate) {
            ASSERT(deltaX == 0);
            ASSERT(deltaY == 0);
            ASSERT(velocityX == 0);
            ASSERT(velocityY == 0);
            ASSERT(!preventPropagation);
        }

        if (type() == PlatformEvent::GestureScrollBegin)
            DCHECK_NE(ScrollInertialPhaseMomentum, inertialPhase);

        m_data.m_scroll.m_deltaX = deltaX;
        m_data.m_scroll.m_deltaY = deltaY;
        m_data.m_scroll.m_deltaUnits = deltaUnits;
        m_data.m_scroll.m_velocityX = velocityX;
        m_data.m_scroll.m_velocityY = velocityY;
        m_data.m_scroll.m_inertialPhase = inertialPhase;
        m_data.m_scroll.m_resendingPluginId = resendingPluginId;
        m_data.m_scroll.m_preventPropagation = preventPropagation;
    }

    const IntPoint& position() const { return m_position; } // PlatformWindow coordinates.
    const IntPoint& globalPosition() const { return m_globalPosition; } // Screen coordinates.

    const IntSize& area() const { return m_area; }

    PlatformGestureSource source() const { return m_source; }

    float deltaX() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollBegin || m_type == PlatformEvent::GestureScrollUpdate);
        return m_data.m_scroll.m_deltaX;
    }

    float deltaY() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollBegin || m_type == PlatformEvent::GestureScrollUpdate);
        return m_data.m_scroll.m_deltaY;
    }

    ScrollGranularity deltaUnits() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollBegin || m_type == PlatformEvent::GestureScrollUpdate || m_type == PlatformEvent::GestureScrollEnd);
        return m_data.m_scroll.m_deltaUnits;
    }

    int tapCount() const
    {
        ASSERT(m_type == PlatformEvent::GestureTap);
        return m_data.m_tap.m_tapCount;
    }

    float velocityX() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollUpdate || m_type == PlatformEvent::GestureFlingStart);
        return m_data.m_scroll.m_velocityX;
    }

    float velocityY() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollUpdate || m_type == PlatformEvent::GestureFlingStart);
        return m_data.m_scroll.m_velocityY;
    }

    ScrollInertialPhase inertialPhase() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollBegin || m_type == PlatformEvent::GestureScrollUpdate || m_type == PlatformEvent::GestureScrollEnd);
        return m_data.m_scroll.m_inertialPhase;
    }

    bool synthetic() const
    {
        ASSERT(m_type == PlatformEvent::GestureScrollBegin || m_type == PlatformEvent::GestureScrollEnd);
        return m_data.m_scroll.m_synthetic;
    }

    int resendingPluginId() const
    {
        if (m_type == PlatformEvent::GestureScrollUpdate
            || m_type == PlatformEvent::GestureScrollBegin
            || m_type == PlatformEvent::GestureScrollEnd)
            return m_data.m_scroll.m_resendingPluginId;

        // This function is called by *all* gesture event types in
        // GestureEvent::Create(), so we return -1 for all other types.
        return -1;
    }

    bool preventPropagation() const
    {
        // TODO(tdresser) Once we've decided if we're getting rid of scroll
        // chaining, we should remove all scroll chaining related logic. See
        // crbug.com/526462 for details.
        ASSERT(m_type == PlatformEvent::GestureScrollUpdate);
        return true;
    }

    float scale() const
    {
        ASSERT(m_type == PlatformEvent::GesturePinchUpdate);
        return m_data.m_pinchUpdate.m_scale;
    }

    void applyTouchAdjustment(const IntPoint& adjustedPosition)
    {
        // Update the window-relative position of the event so that the node that was
        // ultimately hit is under this point (i.e. elementFromPoint for the client
        // co-ordinates in a 'click' event should yield the target). The global
        // position is intentionally left unmodified because it's intended to reflect
        // raw co-ordinates unrelated to any content.
        m_position = adjustedPosition;
    }

    bool isScrollEvent() const
    {
        switch (m_type) {
        case GestureScrollBegin:
        case GestureScrollEnd:
        case GestureScrollUpdate:
        case GestureFlingStart:
        case GesturePinchBegin:
        case GesturePinchEnd:
        case GesturePinchUpdate:
            return true;
        case GestureTap:
        case GestureTapUnconfirmed:
        case GestureTapDown:
        case GestureShowPress:
        case GestureTapDownCancel:
        case GestureTwoFingerTap:
        case GestureLongPress:
        case GestureLongTap:
            return false;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    uint32_t uniqueTouchEventId() const { return m_uniqueTouchEventId; }

protected:
    IntPoint m_position;
    IntPoint m_globalPosition;
    IntSize m_area;
    PlatformGestureSource m_source;

    union {
        struct {
            int m_tapCount;
        } m_tap;

        struct {
            // |m_deltaX| and |m_deltaY| represent deltas in GSU but
            // are only hints in GSB.
            float m_deltaX;
            float m_deltaY;
            float m_velocityX;
            float m_velocityY;
            int m_preventPropagation;
            ScrollInertialPhase m_inertialPhase;
            ScrollGranularity m_deltaUnits;
            int m_resendingPluginId;
            bool m_synthetic;
        } m_scroll;

        struct {
            float m_scale;
        } m_pinchUpdate;
    } m_data;

    uint32_t m_uniqueTouchEventId;
};

} // namespace blink

#endif // PlatformGestureEvent_h
