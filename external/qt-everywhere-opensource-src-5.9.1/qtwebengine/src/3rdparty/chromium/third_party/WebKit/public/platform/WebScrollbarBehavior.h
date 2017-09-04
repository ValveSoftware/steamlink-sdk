// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScrollbarBehavior_h
#define WebScrollbarBehavior_h

#include "WebPointerProperties.h"

namespace blink {

struct WebPoint;
struct WebRect;

class WebScrollbarBehavior {
 public:
  virtual ~WebScrollbarBehavior() {}
  virtual bool shouldCenterOnThumb(WebPointerProperties::Button,
                                   bool shiftKeyPressed,
                                   bool altKeyPressed) {
    return false;
  }
  virtual bool shouldSnapBackToDragOrigin(const WebPoint& eventPoint,
                                          const WebRect& scrollbarRect,
                                          bool isHorizontal) {
    return false;
  }
};

}  // namespace blink

#endif
