/**
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/events/KeyboardEvent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/WindowsKeyboardCodes.h"
#include "wtf/PtrUtil.h"

namespace blink {

static inline const AtomicString& eventTypeForKeyboardEventType(PlatformEvent::EventType type)
{
    switch (type) {
        case PlatformEvent::KeyUp:
            return EventTypeNames::keyup;
        case PlatformEvent::RawKeyDown:
            return EventTypeNames::keydown;
        case PlatformEvent::Char:
            return EventTypeNames::keypress;
        case PlatformEvent::KeyDown:
            // The caller should disambiguate the combined event into RawKeyDown or Char events.
            break;
        default:
            break;
    }
    ASSERT_NOT_REACHED();
    return EventTypeNames::keydown;
}

static inline KeyboardEvent::KeyLocationCode keyLocationCode(const PlatformKeyboardEvent& key)
{
    if (key.isKeypad())
        return KeyboardEvent::DOM_KEY_LOCATION_NUMPAD;
    if (key.getModifiers() & PlatformEvent::IsLeft)
        return KeyboardEvent::DOM_KEY_LOCATION_LEFT;
    if (key.getModifiers() & PlatformEvent::IsRight)
        return KeyboardEvent::DOM_KEY_LOCATION_RIGHT;
    return KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
}

KeyboardEvent* KeyboardEvent::create(ScriptState* scriptState, const AtomicString& type, const KeyboardEventInit& initializer)
{
    if (scriptState->world().isIsolatedWorld())
        UIEventWithKeyState::didCreateEventInIsolatedWorld(initializer.ctrlKey(), initializer.altKey(), initializer.shiftKey(), initializer.metaKey());
    return new KeyboardEvent(type, initializer);
}

KeyboardEvent::KeyboardEvent()
    : m_location(DOM_KEY_LOCATION_STANDARD)
{
}

KeyboardEvent::KeyboardEvent(const PlatformKeyboardEvent& key, AbstractView* view)
    : UIEventWithKeyState(eventTypeForKeyboardEventType(key.type()), true, true, view, 0, key.getModifiers(), key.timestamp(), InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities())
    , m_keyEvent(wrapUnique(new PlatformKeyboardEvent(key)))
    , m_keyIdentifier(key.keyIdentifier())
    , m_code(key.code())
    , m_key(key.key())
    , m_location(keyLocationCode(key))
{
    initLocationModifiers(m_location);
}

KeyboardEvent::KeyboardEvent(const AtomicString& eventType, const KeyboardEventInit& initializer)
    : UIEventWithKeyState(eventType, initializer)
    , m_keyIdentifier(initializer.keyIdentifier())
    , m_code(initializer.code())
    , m_key(initializer.key())
    , m_location(initializer.location())
{
    if (initializer.repeat())
        m_modifiers |= PlatformEvent::IsAutoRepeat;
    initLocationModifiers(initializer.location());
}

KeyboardEvent::KeyboardEvent(const AtomicString& eventType, bool canBubble, bool cancelable, AbstractView* view,
    const String& keyIdentifier, const String& code, const String& key, unsigned location, PlatformEvent::Modifiers modifiers,
    double plaformTimeStamp)
    : UIEventWithKeyState(eventType, canBubble, cancelable, view, 0, modifiers, plaformTimeStamp, InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities())
    , m_keyIdentifier(keyIdentifier)
    , m_code(code)
    , m_key(key)
    , m_location(location)
{
    initLocationModifiers(location);
}

KeyboardEvent::~KeyboardEvent()
{
}

void KeyboardEvent::initKeyboardEvent(ScriptState* scriptState, const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
    const String& keyIdentifier, unsigned location, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    if (isBeingDispatched())
        return;

    if (scriptState->world().isIsolatedWorld())
        UIEventWithKeyState::didCreateEventInIsolatedWorld(ctrlKey, altKey, shiftKey, metaKey);

    initUIEvent(type, canBubble, cancelable, view, 0);

    m_keyIdentifier = keyIdentifier;
    m_location = location;
    initModifiers(ctrlKey, altKey, shiftKey, metaKey);
    initLocationModifiers(location);
}

int KeyboardEvent::keyCode() const
{
    // IE: virtual key code for keyup/keydown, character code for keypress
    // Firefox: virtual key code for keyup/keydown, zero for keypress
    // We match IE.
    if (!m_keyEvent)
        return 0;

#if OS(ANDROID)
    // FIXME: Check to see if this applies to other OS.
    // If the key event belongs to IME composition then propagate to JS.
    if (m_keyEvent->nativeVirtualKeyCode() == 0xE5) // VKEY_PROCESSKEY
        return m_keyEvent->nativeVirtualKeyCode();
#endif

    if (type() == EventTypeNames::keydown || type() == EventTypeNames::keyup)
        return m_keyEvent->windowsVirtualKeyCode();

    return charCode();
}

int KeyboardEvent::charCode() const
{
    // IE: not supported
    // Firefox: 0 for keydown/keyup events, character code for keypress
    // We match Firefox

    if (!m_keyEvent || (type() != EventTypeNames::keypress))
        return 0;
    String text = m_keyEvent->text();
    return static_cast<int>(text.characterStartingAt(0));
}

const AtomicString& KeyboardEvent::interfaceName() const
{
    return EventNames::KeyboardEvent;
}

bool KeyboardEvent::isKeyboardEvent() const
{
    return true;
}

int KeyboardEvent::which() const
{
    // Netscape's "which" returns a virtual key code for keydown and keyup, and a character code for keypress.
    // That's exactly what IE's "keyCode" returns. So they are the same for keyboard events.
    return keyCode();
}

void KeyboardEvent::initLocationModifiers(unsigned location)
{
    switch (location) {
    case KeyboardEvent::DOM_KEY_LOCATION_NUMPAD:
        m_modifiers |= PlatformEvent::IsKeyPad;
        break;
    case KeyboardEvent::DOM_KEY_LOCATION_LEFT:
        m_modifiers |= PlatformEvent::IsLeft;
        break;
    case KeyboardEvent::DOM_KEY_LOCATION_RIGHT:
        m_modifiers |= PlatformEvent::IsRight;
        break;
    }
}

DEFINE_TRACE(KeyboardEvent)
{
    UIEventWithKeyState::trace(visitor);
}

} // namespace blink
