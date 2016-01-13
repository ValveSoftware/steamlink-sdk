// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBSCROLLBARBEHAVIOR_IMPL_GTKORAURA_H_
#define CONTENT_CHILD_WEBSCROLLBARBEHAVIOR_IMPL_GTKORAURA_H_

#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"

namespace content {

class WebScrollbarBehaviorImpl : public blink::WebScrollbarBehavior {
 public:
  virtual bool shouldCenterOnThumb(
      blink::WebScrollbarBehavior::Button mouseButton,
      bool shiftKeyPressed,
      bool altKeyPressed);
  virtual bool shouldSnapBackToDragOrigin(const blink::WebPoint& eventPoint,
                                          const blink::WebRect& scrollbarRect,
                                          bool isHorizontal);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBSCROLLBARBEHAVIOR_IMPL_GTKORAURA_H_
