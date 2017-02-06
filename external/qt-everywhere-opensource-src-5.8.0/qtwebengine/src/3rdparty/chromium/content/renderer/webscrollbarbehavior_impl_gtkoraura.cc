// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webscrollbarbehavior_impl_gtkoraura.h"

#include "build/build_config.h"
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
  static const int kDefaultWinScrollbarThickness = 17;

  // Find the rect within which we shouldn't snap, by expanding the track rect
  // in both dimensions.
  gfx::Rect noSnapRect(scrollbarRect);
  int thickness = isHorizontal ? noSnapRect.height() : noSnapRect.width();
  // Even if the platform's scrollbar is narrower than the default Windows one,
  // we still want to provide at least as much slop area, since a slightly
  // narrower scrollbar doesn't necessarily imply that users will drag
  // straighter.
  thickness = std::max(thickness, kDefaultWinScrollbarThickness);
  noSnapRect.Inset(
      (isHorizontal ? kOffEndMultiplier : kOffSideMultiplier) * -thickness,
      (isHorizontal ? kOffSideMultiplier : kOffEndMultiplier) * -thickness);

  // On most platforms, we should snap iff the event is outside our calculated
  // rect.  On Linux, however, we should not snap for events off the ends, but
  // not the sides, of the rect.
#if (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  return isHorizontal ?
      (eventPoint.y < noSnapRect.y() || eventPoint.y >= noSnapRect.bottom()) :
      (eventPoint.x < noSnapRect.x() || eventPoint.x >= noSnapRect.right());
#else
  return !noSnapRect.Contains(eventPoint);
#endif
}

}  // namespace content
