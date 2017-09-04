// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/scoped_web_input_event.h"

#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebInputEvent;

namespace ui {

WebInputEventDeleter::WebInputEventDeleter() {}

void WebInputEventDeleter::operator()(WebInputEvent* web_event) const {
  WebInputEventTraits::Delete(web_event);
}

}  // namespace ui
