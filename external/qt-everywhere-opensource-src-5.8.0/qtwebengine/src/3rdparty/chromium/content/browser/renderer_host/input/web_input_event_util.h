// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_

#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
enum class DomCode;
struct GestureEventData;
struct GestureEventDetails;
}

namespace content {

int WebEventModifiersToEventFlags(int modifiers);

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(
    ui::DomCode code);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_UTIL_H_
