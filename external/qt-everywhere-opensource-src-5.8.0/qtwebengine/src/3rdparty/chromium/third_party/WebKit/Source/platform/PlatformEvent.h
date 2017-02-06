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

#ifndef PlatformEvent_h
#define PlatformEvent_h

#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PlatformEvent {
    DISALLOW_NEW();
public:
    enum EventType {
        NoType = 0,

        // PlatformKeyboardEvent
        KeyDown,
        KeyUp,
        RawKeyDown,
        Char,

        // PlatformMouseEvent
        MouseMoved,
        MousePressed,
        MouseReleased,
        MouseScroll,

        // PlatformWheelEvent
        Wheel,

        // PlatformGestureEvent
        GestureScrollBegin,
        GestureScrollEnd,
        GestureScrollUpdate,
        GestureTap,
        GestureTapUnconfirmed,
        GestureTapDown,
        GestureShowPress,
        GestureTapDownCancel,
        GestureTwoFingerTap,
        GestureLongPress,
        GestureLongTap,
        GesturePinchBegin,
        GesturePinchEnd,
        GesturePinchUpdate,
        GestureFlingStart,

        // PlatformTouchEvent
        TouchStart,
        TouchMove,
        TouchEnd,
        TouchCancel,
        TouchScrollStarted,
    };

    // These values are direct mappings of the values in WebInputEvent so the values can be cast between the
    // enumerations. static_asserts checking this are in web/WebInputEventConversion.cpp.
    enum Modifiers {
        NoModifiers = 0,
        ShiftKey    = 1 << 0,
        CtrlKey     = 1 << 1,
        AltKey      = 1 << 2,
        MetaKey     = 1 << 3,

        IsKeyPad     = 1 << 4,
        IsAutoRepeat = 1 << 5,

        LeftButtonDown   = 1 << 6,
        MiddleButtonDown = 1 << 7,
        RightButtonDown  = 1 << 8,

        CapsLockOn = 1 << 9,
        NumLockOn  = 1 << 10,

        IsLeft      = 1 << 11,
        IsRight     = 1 << 12,
        IsTouchAccessibility = 1 << 13,
        IsComposing = 1 << 14,

        AltGrKey  = 1 << 15,
        FnKey     = 1 << 16,
        SymbolKey = 1 << 17,

        ScrollLockOn = 1 << 18,

        // The set of non-stateful modifiers that specifically change the
        // interpretation of the key being pressed. For example; IsLeft,
        // IsRight, IsComposing don't change the meaning of the key
        // being pressed. NumLockOn, ScrollLockOn, CapsLockOn are stateful
        // and don't indicate explicit depressed state.
        KeyModifiers = SymbolKey | FnKey | AltGrKey | MetaKey | AltKey | CtrlKey | ShiftKey,
    };

    enum RailsMode {
        RailsModeFree       = 0,
        RailsModeHorizontal = 1,
        RailsModeVertical   = 2,
    };

    // These values are direct mappings of the values in WebInputEvent
    // so the values can be cast between the enumerations. static_asserts
    // checking this are in web/WebInputEventConversion.cpp.
    enum DispatchType {
        Blocking,
        EventNonBlocking,
        // All listeners are passive.
        ListenersNonBlockingPassive,
        // This value represents a state which would have normally blocking
        // but was forced to be non-blocking.
        ListenersForcedNonBlockingPassive,
    };

    EventType type() const { return static_cast<EventType>(m_type); }

    bool shiftKey() const { return m_modifiers & ShiftKey; }
    bool ctrlKey() const { return m_modifiers & CtrlKey; }
    bool altKey() const { return m_modifiers & AltKey; }
    bool metaKey() const { return m_modifiers & MetaKey; }

    Modifiers getModifiers() const { return static_cast<Modifiers>(m_modifiers); }

    double timestamp() const { return m_timestamp; }

protected:
    PlatformEvent()
        : m_type(NoType)
        , m_modifiers()
        , m_timestamp(0)
    {
    }

    explicit PlatformEvent(EventType type)
        : m_type(type)
        , m_modifiers(0)
        , m_timestamp(0)
    {
    }

    PlatformEvent(EventType type, Modifiers modifiers, double timestamp)
        : m_type(type)
        , m_modifiers(modifiers)
        , m_timestamp(timestamp)
    {
    }

    // Explicit protected destructor so that people don't accidentally
    // delete a PlatformEvent.
    ~PlatformEvent()
    {
    }

    unsigned m_type;
    unsigned m_modifiers;
    double m_timestamp;
};

} // namespace blink

#endif // PlatformEvent_h
