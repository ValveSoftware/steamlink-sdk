// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_GESTURE_EVENT_TYPE_
#define CONTENT_BROWSER_ANDROID_GESTURE_EVENT_TYPE_

namespace content {

enum GestureEventType {
#define DEFINE_GESTURE_EVENT_TYPE(name, value) name = value,
#include "content/browser/android/gesture_event_type_list.h"
#undef DEFINE_GESTURE_EVENT_TYPE
};

} // namespace content

#endif  // CONTENT_BROWSER_ANDROID_GESTURE_EVENT_TYPE_

