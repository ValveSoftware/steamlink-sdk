// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_SCOPED_WEB_INPUT_EVENT_H_
#define UI_EVENTS_BLINK_SCOPED_WEB_INPUT_EVENT_H_

#include <memory>

namespace blink {
class WebInputEvent;
}

namespace ui {

// blink::WebInputEvent does not provide a virtual destructor.
struct WebInputEventDeleter {
  WebInputEventDeleter();
  void operator()(blink::WebInputEvent* web_event) const;
};
typedef std::unique_ptr<blink::WebInputEvent, WebInputEventDeleter>
    ScopedWebInputEvent;

}  // namespace ui

#endif  // UI_EVENTS_BLINK_SCOPED_WEB_INPUT_EVENT_H_
