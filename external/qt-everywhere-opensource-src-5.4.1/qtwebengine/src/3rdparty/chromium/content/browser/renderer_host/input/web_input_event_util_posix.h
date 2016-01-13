// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_POSIX_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_POSIX_H_

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

ui::KeyboardCode GetWindowsKeyCodeWithoutLocation(ui::KeyboardCode key_code);
blink::WebInputEvent::Modifiers GetLocationModifiersFromWindowsKeyCode(
    ui::KeyboardCode key_code);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_POSIX_H_
