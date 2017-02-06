// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SCOPED_WEB_INPUT_EVENT_H_
#define CONTENT_COMMON_INPUT_SCOPED_WEB_INPUT_EVENT_H_

#include <memory>

#include "content/common/content_export.h"

namespace blink {
class WebInputEvent;
}

namespace content {

// blink::WebInputEvent does not provide a virtual destructor.
struct CONTENT_EXPORT WebInputEventDeleter {
  WebInputEventDeleter();
  void operator()(blink::WebInputEvent* web_event) const;
};
typedef std::unique_ptr<blink::WebInputEvent, WebInputEventDeleter>
    ScopedWebInputEvent;

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SCOPED_WEB_INPUT_EVENT_H_
