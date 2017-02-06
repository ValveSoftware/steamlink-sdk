/*
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef PlatformMouseEvent_h
#define PlatformMouseEvent_h

#include "platform/PlatformEvent.h"
#include "platform/geometry/IntPoint.h"
#include "public/platform/WebPointerProperties.h"
#include "wtf/text/WTFString.h"

namespace blink {

// These button numbers match the ones used in the DOM API, 0 through 2, except for NoButton which is specified in PointerEvent
// spec but not in MouseEvent spec.
enum MouseButton { NoButton = -1, LeftButton, MiddleButton, RightButton };

class PlatformMouseEvent : public PlatformEvent {
public:
    enum SyntheticEventType {
        // Real mouse input events or synthetic events that behave just like real events
        RealOrIndistinguishable,
        // Synthetic mouse events derived from touch input
        FromTouch,
        // Synthetic mouse events generated without a position, for example those generated
        // from keyboard input.
        Positionless,
    };

    PlatformMouseEvent()
        : PlatformEvent(PlatformEvent::MouseMoved)
        , m_button(NoButton)
        , m_clickCount(0)
        , m_synthesized(RealOrIndistinguishable)
    {
    }

    PlatformMouseEvent(const IntPoint& position, const IntPoint& globalPosition, MouseButton button, EventType type, int clickCount, Modifiers modifiers, double timestamp)
        : PlatformEvent(type, modifiers, timestamp)
        , m_position(position)
        , m_globalPosition(globalPosition)
        , m_button(button)
        , m_clickCount(clickCount)
        , m_synthesized(RealOrIndistinguishable)
    {
    }

    PlatformMouseEvent(const IntPoint& position, const IntPoint& globalPosition, MouseButton button, EventType type, int clickCount, Modifiers modifiers, SyntheticEventType synthesized, double timestamp, WebPointerProperties::PointerType pointerType = WebPointerProperties::PointerType::Unknown)
        : PlatformEvent(type, modifiers, timestamp)
        , m_position(position)
        , m_globalPosition(globalPosition)
        , m_button(button)
        , m_clickCount(clickCount)
        , m_synthesized(synthesized)
    {
        m_pointerProperties.pointerType = pointerType;
    }

    const WebPointerProperties& pointerProperties() const { return m_pointerProperties; }
    const IntPoint& position() const { return m_position; }
    const IntPoint& globalPosition() const { return m_globalPosition; }
    const IntPoint& movementDelta() const { return m_movementDelta; }

    MouseButton button() const { return m_button; }
    int clickCount() const { return m_clickCount; }
    bool fromTouch() const { return m_synthesized == FromTouch; }
    SyntheticEventType getSyntheticEventType() const { return m_synthesized; }

    const String& region() const { return m_region; }
    void setRegion(const String& region) { m_region = region; }

protected:
    WebPointerProperties m_pointerProperties;

    // In local root frame coordinates. (Except possibly if the Widget under
    // the mouse is a popup, see FIXME in PlatformMouseEventBuilder).
    IntPoint m_position;

    // In screen coordinates.
    IntPoint m_globalPosition;

    IntPoint m_movementDelta;
    MouseButton m_button;
    int m_clickCount;
    SyntheticEventType m_synthesized;

    // For canvas hit region.
    // TODO(zino): This might make more sense to put in HitTestResults or
    // some other part of MouseEventWithHitTestResults, but for now it's
    // most convenient to stash it here. Please see: http://crbug.com/592947.
    String m_region;
};

} // namespace blink

#endif // PlatformMouseEvent_h
