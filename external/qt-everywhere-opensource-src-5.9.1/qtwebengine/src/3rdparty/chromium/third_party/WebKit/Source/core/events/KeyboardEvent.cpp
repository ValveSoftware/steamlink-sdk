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
#include "core/editing/InputMethodController.h"
#include "platform/WindowsKeyboardCodes.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

const AtomicString& eventTypeForKeyboardEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::KeyUp:
      return EventTypeNames::keyup;
    case WebInputEvent::RawKeyDown:
      return EventTypeNames::keydown;
    case WebInputEvent::Char:
      return EventTypeNames::keypress;
    case WebInputEvent::KeyDown:
      // The caller should disambiguate the combined event into RawKeyDown or
      // Char events.
      break;
    default:
      break;
  }
  NOTREACHED();
  return EventTypeNames::keydown;
}

KeyboardEvent::KeyLocationCode keyLocationCode(const WebInputEvent& key) {
  if (key.modifiers & WebInputEvent::IsKeyPad)
    return KeyboardEvent::kDomKeyLocationNumpad;
  if (key.modifiers & WebInputEvent::IsLeft)
    return KeyboardEvent::kDomKeyLocationLeft;
  if (key.modifiers & WebInputEvent::IsRight)
    return KeyboardEvent::kDomKeyLocationRight;
  return KeyboardEvent::kDomKeyLocationStandard;
}

bool hasCurrentComposition(LocalDOMWindow* domWindow) {
  if (!domWindow)
    return false;
  LocalFrame* localFrame = domWindow->frame();
  if (!localFrame)
    return false;
  return localFrame->inputMethodController().hasComposition();
}

}  // namespace

KeyboardEvent* KeyboardEvent::create(ScriptState* scriptState,
                                     const AtomicString& type,
                                     const KeyboardEventInit& initializer) {
  if (scriptState->world().isIsolatedWorld())
    UIEventWithKeyState::didCreateEventInIsolatedWorld(
        initializer.ctrlKey(), initializer.altKey(), initializer.shiftKey(),
        initializer.metaKey());
  return new KeyboardEvent(type, initializer);
}

KeyboardEvent::KeyboardEvent() : m_location(kDomKeyLocationStandard) {}

KeyboardEvent::KeyboardEvent(const WebKeyboardEvent& key,
                             LocalDOMWindow* domWindow)
    : UIEventWithKeyState(
          eventTypeForKeyboardEventType(key.type),
          true,
          true,
          domWindow,
          0,
          static_cast<PlatformEvent::Modifiers>(key.modifiers),
          key.timeStampSeconds,
          InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities()),
      m_keyEvent(makeUnique<WebKeyboardEvent>(key)),
      // TODO(crbug.com/482880): Fix this initialization to lazy initialization.
      m_code(Platform::current()->domCodeStringFromEnum(key.domCode)),
      m_key(Platform::current()->domKeyStringFromEnum(key.domKey)),
      m_location(keyLocationCode(key)),
      m_isComposing(hasCurrentComposition(domWindow)) {
  initLocationModifiers(m_location);
}

KeyboardEvent::KeyboardEvent(const AtomicString& eventType,
                             const KeyboardEventInit& initializer)
    : UIEventWithKeyState(eventType, initializer),
      m_code(initializer.code()),
      m_key(initializer.key()),
      m_location(initializer.location()),
      m_isComposing(initializer.isComposing()) {
  if (initializer.repeat())
    m_modifiers |= PlatformEvent::IsAutoRepeat;
  initLocationModifiers(initializer.location());
}

KeyboardEvent::~KeyboardEvent() {}

void KeyboardEvent::initKeyboardEvent(ScriptState* scriptState,
                                      const AtomicString& type,
                                      bool canBubble,
                                      bool cancelable,
                                      AbstractView* view,
                                      const String& keyIdentifier,
                                      unsigned location,
                                      bool ctrlKey,
                                      bool altKey,
                                      bool shiftKey,
                                      bool metaKey) {
  if (isBeingDispatched())
    return;

  if (scriptState->world().isIsolatedWorld())
    UIEventWithKeyState::didCreateEventInIsolatedWorld(ctrlKey, altKey,
                                                       shiftKey, metaKey);

  initUIEvent(type, canBubble, cancelable, view, 0);

  m_location = location;
  initModifiers(ctrlKey, altKey, shiftKey, metaKey);
  initLocationModifiers(location);
}

int KeyboardEvent::keyCode() const {
  // IE: virtual key code for keyup/keydown, character code for keypress
  // Firefox: virtual key code for keyup/keydown, zero for keypress
  // We match IE.
  if (!m_keyEvent)
    return 0;

#if OS(ANDROID)
  // FIXME: Check to see if this applies to other OS.
  // If the key event belongs to IME composition then propagate to JS.
  if (m_keyEvent->nativeKeyCode == 0xE5)  // VKEY_PROCESSKEY
    return m_keyEvent->nativeKeyCode;
#endif

  if (type() == EventTypeNames::keydown || type() == EventTypeNames::keyup)
    return m_keyEvent->windowsKeyCode;

  return charCode();
}

int KeyboardEvent::charCode() const {
  // IE: not supported
  // Firefox: 0 for keydown/keyup events, character code for keypress
  // We match Firefox

  if (!m_keyEvent || (type() != EventTypeNames::keypress))
    return 0;
  return m_keyEvent->text[0];
}

const AtomicString& KeyboardEvent::interfaceName() const {
  return EventNames::KeyboardEvent;
}

bool KeyboardEvent::isKeyboardEvent() const {
  return true;
}

int KeyboardEvent::which() const {
  // Netscape's "which" returns a virtual key code for keydown and keyup, and a
  // character code for keypress.  That's exactly what IE's "keyCode" returns.
  // So they are the same for keyboard events.
  return keyCode();
}

void KeyboardEvent::initLocationModifiers(unsigned location) {
  switch (location) {
    case KeyboardEvent::kDomKeyLocationNumpad:
      m_modifiers |= PlatformEvent::IsKeyPad;
      break;
    case KeyboardEvent::kDomKeyLocationLeft:
      m_modifiers |= PlatformEvent::IsLeft;
      break;
    case KeyboardEvent::kDomKeyLocationRight:
      m_modifiers |= PlatformEvent::IsRight;
      break;
  }
}

DEFINE_TRACE(KeyboardEvent) {
  UIEventWithKeyState::trace(visitor);
}

}  // namespace blink
