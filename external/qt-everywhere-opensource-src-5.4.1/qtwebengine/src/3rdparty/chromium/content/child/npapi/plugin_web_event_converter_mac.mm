// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "content/child/npapi/plugin_web_event_converter_mac.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {

namespace {

// Returns true if the given key is a modifier key.
bool KeyIsModifier(int native_key_code) {
  switch (native_key_code) {
    case 55:  // Left command
    case 54:  // Right command
    case 58:  // Left option
    case 61:  // Right option
    case 59:  // Left control
    case 62:  // Right control
    case 56:  // Left shift
    case 60:  // Right shift
    case 57:  // Caps lock
      return true;
    default:
      return false;
  }
}

// Returns true if the caps lock flag should be set for the given event.
bool CapsLockIsActive(const WebInputEvent& event) {
  // Only key events have accurate information for the caps lock flag; see
  // <https://bugs.webkit.org/show_bug.cgi?id=46518>.
  // For other types, use the live state.
  if (WebInputEvent::isKeyboardEventType(event.type))
    return (event.modifiers & WebInputEvent::CapsLockOn) != 0;
  else
    return ([[NSApp currentEvent] modifierFlags] & NSAlphaShiftKeyMask) != 0;
}

}  // namespace

#pragma mark -

PluginWebEventConverter::PluginWebEventConverter() {
}

PluginWebEventConverter::~PluginWebEventConverter() {
}

bool PluginWebEventConverter::InitWithEvent(const WebInputEvent& web_event) {
  memset(&cocoa_event_, 0, sizeof(cocoa_event_));
  if (web_event.type == WebInputEvent::MouseWheel) {
    return ConvertMouseWheelEvent(
        *static_cast<const WebMouseWheelEvent*>(&web_event));
  } else if (WebInputEvent::isMouseEventType(web_event.type)) {
    return ConvertMouseEvent(*static_cast<const WebMouseEvent*>(&web_event));
  } else if (WebInputEvent::isKeyboardEventType(web_event.type)) {
    return ConvertKeyboardEvent(
       *static_cast<const WebKeyboardEvent*>(&web_event));
  }
  DLOG(WARNING) << "Unknown event type " << web_event.type;
  return false;
}

bool PluginWebEventConverter::ConvertKeyboardEvent(
    const WebKeyboardEvent& key_event) {
  cocoa_event_.data.key.keyCode = key_event.nativeKeyCode;

  cocoa_event_.data.key.modifierFlags |= CocoaModifiers(key_event);

  // Modifier keys have their own event type, and don't get character or
  // repeat data.
  if (KeyIsModifier(key_event.nativeKeyCode)) {
    cocoa_event_.type = NPCocoaEventFlagsChanged;
    return true;
  }

  cocoa_event_.data.key.characters = reinterpret_cast<NPNSString*>(
      [NSString stringWithFormat:@"%S", key_event.text]);
  cocoa_event_.data.key.charactersIgnoringModifiers =
      reinterpret_cast<NPNSString*>(
          [NSString stringWithFormat:@"%S", key_event.unmodifiedText]);

  if (key_event.modifiers & WebInputEvent::IsAutoRepeat)
    cocoa_event_.data.key.isARepeat = true;

  switch (key_event.type) {
    case WebInputEvent::KeyDown:
      cocoa_event_.type = NPCocoaEventKeyDown;
      return true;
    case WebInputEvent::KeyUp:
      cocoa_event_.type = NPCocoaEventKeyUp;
      return true;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::Char:
      // May be used eventually for IME, but currently not needed.
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool PluginWebEventConverter::ConvertMouseEvent(
    const WebMouseEvent& mouse_event) {
  cocoa_event_.data.mouse.pluginX = mouse_event.x;
  cocoa_event_.data.mouse.pluginY = mouse_event.y;
  cocoa_event_.data.mouse.modifierFlags |= CocoaModifiers(mouse_event);
  cocoa_event_.data.mouse.clickCount = mouse_event.clickCount;
  switch (mouse_event.button) {
    case WebMouseEvent::ButtonLeft:
      cocoa_event_.data.mouse.buttonNumber = 0;
      break;
    case WebMouseEvent::ButtonMiddle:
      cocoa_event_.data.mouse.buttonNumber = 2;
      break;
    case WebMouseEvent::ButtonRight:
      cocoa_event_.data.mouse.buttonNumber = 1;
      break;
    default:
      cocoa_event_.data.mouse.buttonNumber = mouse_event.button;
      break;
  }
  switch (mouse_event.type) {
    case WebInputEvent::MouseDown:
      cocoa_event_.type = NPCocoaEventMouseDown;
      return true;
    case WebInputEvent::MouseUp:
      cocoa_event_.type = NPCocoaEventMouseUp;
      return true;
    case WebInputEvent::MouseMove: {
      bool mouse_is_down =
          (mouse_event.modifiers & WebInputEvent::LeftButtonDown) ||
          (mouse_event.modifiers & WebInputEvent::RightButtonDown) ||
          (mouse_event.modifiers & WebInputEvent::MiddleButtonDown);
      cocoa_event_.type = mouse_is_down ? NPCocoaEventMouseDragged
                                        : NPCocoaEventMouseMoved;
      return true;
    }
    case WebInputEvent::MouseEnter:
      cocoa_event_.type = NPCocoaEventMouseEntered;
      return true;
    case WebInputEvent::MouseLeave:
      cocoa_event_.type = NPCocoaEventMouseExited;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool PluginWebEventConverter::ConvertMouseWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  cocoa_event_.type = NPCocoaEventScrollWheel;
  cocoa_event_.data.mouse.pluginX = wheel_event.x;
  cocoa_event_.data.mouse.pluginY = wheel_event.y;
  cocoa_event_.data.mouse.modifierFlags |= CocoaModifiers(wheel_event);
  cocoa_event_.data.mouse.deltaX = wheel_event.deltaX;
  cocoa_event_.data.mouse.deltaY = wheel_event.deltaY;
  return true;
}

NSUInteger PluginWebEventConverter::CocoaModifiers(
    const WebInputEvent& web_event) {
  NSInteger modifiers = 0;
  if (web_event.modifiers & WebInputEvent::ControlKey)
    modifiers |= NSControlKeyMask;
  if (web_event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= NSShiftKeyMask;
  if (web_event.modifiers & WebInputEvent::AltKey)
    modifiers |= NSAlternateKeyMask;
  if (web_event.modifiers & WebInputEvent::MetaKey)
    modifiers |= NSCommandKeyMask;
  if (CapsLockIsActive(web_event))
    modifiers |= NSAlphaShiftKeyMask;
  return modifiers;
}

}  // namespace content
