// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_TRAITS_H_
#define CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_TRAITS_H_

#include "base/basictypes.h"
#include "content/common/input/scoped_web_input_event.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// Utility class for performing operations on and with WebInputEvents.
class CONTENT_EXPORT WebInputEventTraits {
 public:
  static const char* GetName(blink::WebInputEvent::Type type);
  static size_t GetSize(blink::WebInputEvent::Type type);
  static ScopedWebInputEvent Clone(const blink::WebInputEvent& event);
  static void Delete(blink::WebInputEvent* event);
  static bool CanCoalesce(const blink::WebInputEvent& event_to_coalesce,
                          const blink::WebInputEvent& event);
  static void Coalesce(const blink::WebInputEvent& event_to_coalesce,
                       blink::WebInputEvent* event);
  static bool IgnoresAckDisposition(const blink::WebInputEvent& event);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_TRAITS_H_
