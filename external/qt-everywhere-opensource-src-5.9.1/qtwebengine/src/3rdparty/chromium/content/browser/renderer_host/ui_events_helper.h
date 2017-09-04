// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UI_EVENTS_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_UI_EVENTS_HELPER_H_

#include "base/memory/scoped_vector.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"

namespace ui {
class TouchEvent;
}

namespace content {

enum TouchEventCoordinateSystem {
  SCREEN_COORDINATES,
  LOCAL_COORDINATES
};

// Creates a list of ui::TouchEvents out of a single WebTouchEvent.
// A WebTouchEvent can contain information about a number of WebTouchPoints,
// whereas a ui::TouchEvent contains information about a single touch-point. So
// it is possible to create more than one ui::TouchEvents out of a single
// WebTouchEvent. All the ui::TouchEvent in the list will carry the same
// LatencyInfo the WebTouchEvent carries.
// |coordinate_system| specifies which fields to use for the co-ordinates,
// WebTouchPoint.position or WebTouchPoint.screenPosition.  Is's up to the
// caller to do any co-ordinate system mapping (typically to get them into
// the Aura EventDispatcher co-ordinate system).
CONTENT_EXPORT bool MakeUITouchEventsFromWebTouchEvents(
    const TouchEventWithLatencyInfo& touch,
    ScopedVector<ui::TouchEvent>* list,
    TouchEventCoordinateSystem coordinate_system);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UI_EVENTS_HELPER_H_
