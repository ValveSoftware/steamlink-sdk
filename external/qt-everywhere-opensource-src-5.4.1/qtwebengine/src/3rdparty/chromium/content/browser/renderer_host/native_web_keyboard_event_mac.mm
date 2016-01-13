// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#import <AppKit/AppKit.h>

#include "third_party/WebKit/public/web/mac/WebInputEventFactory.h"

using blink::WebInputEventFactory;

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(native_event)),
      os_event([native_event retain]),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(wchar_t character,
                                               int modifiers,
                                               double time_stamp_seconds)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(character,
                                                           modifiers,
                                                           time_stamp_seconds)),
      os_event(NULL),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event([other.os_event retain]),
      skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  NSObject* previous = os_event;
  os_event = [other.os_event retain];
  [previous release];

  skip_in_browser = other.skip_in_browser;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  [os_event release];
}

}  // namespace content
