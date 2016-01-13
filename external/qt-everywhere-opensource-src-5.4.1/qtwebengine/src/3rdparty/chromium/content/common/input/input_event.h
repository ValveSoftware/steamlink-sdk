// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/scoped_web_input_event.h"
#include "ui/events/latency_info.h"

namespace blink {
class WebInputEvent;
}

namespace content {

// An content-specific wrapper for WebInputEvents and associated metadata.
class CONTENT_EXPORT InputEvent {
 public:
  InputEvent();
  InputEvent(const blink::WebInputEvent& web_event,
             const ui::LatencyInfo& latency_info,
             bool is_keyboard_shortcut);
  ~InputEvent();

  ScopedWebInputEvent web_event;
  ui::LatencyInfo latency_info;
  bool is_keyboard_shortcut;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputEvent);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_H_
