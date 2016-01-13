// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event.h"

#include "content/common/input/web_input_event_traits.h"

namespace content {

InputEvent::InputEvent() : is_keyboard_shortcut(false) {}

InputEvent::InputEvent(const blink::WebInputEvent& web_event,
                       const ui::LatencyInfo& latency_info,
                       bool is_keyboard_shortcut)
     : web_event(WebInputEventTraits::Clone(web_event)),
       latency_info(latency_info),
       is_keyboard_shortcut(is_keyboard_shortcut) {}

InputEvent::~InputEvent() {}

}  // namespace content
