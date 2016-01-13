// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webscrollbarbehavior_impl_gtkoraura.h"

#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"

namespace content {

bool WebScrollbarBehaviorImpl::shouldCenterOnThumb(
      blink::WebScrollbarBehavior::Button mouseButton,
      bool shiftKeyPressed,
      bool altKeyPressed) {
#if (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (mouseButton == blink::WebScrollbarBehavior::ButtonMiddle)
    return true;
#endif
  return (mouseButton == blink::WebScrollbarBehavior::ButtonLeft) &&
      shiftKeyPressed;
}

bool WebScrollbarBehaviorImpl::shouldSnapBackToDragOrigin(
    const blink::WebPoint& eventPoint,
    const blink::WebRect& scrollbarRect,
    bool isHorizontal) {
  // Constants used to figure the drag rect outside which we should snap the
  // scrollbar thumb back to its origin. These calculations are based on
  // observing the behavior of the MSVC8 main window scrollbar + some
  // guessing/extrapolation.
  static const int kOffEndMultiplier = 3;
  static const int kOffSideMultiplier = 8;

  // Find the rect within which we shouldn't snap, by expanding the track rect
  // in both dimensions.
  gfx::Rect noSnapRect(scrollbarRect);
  const int thickness = isHorizontal ? noSnapRect.height() : noSnapRect.width();
  noSnapRect.Inset(
      (isHorizontal ? kOffEndMultiplier : kOffSideMultiplier) * -thickness,
      (isHorizontal ? kOffSideMultiplier : kOffEndMultiplier) * -thickness);

  // We should snap iff the event is outside our calculated rect.
  return !noSnapRect.Contains(eventPoint);
}

}  // namespace content
