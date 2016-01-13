// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/strings/stringprintf.h"

namespace gfx {

template class PointBase<Point, int, Vector2d>;

#if defined(OS_WIN)
Point::Point(DWORD point) : PointBase<Point, int, Vector2d>(0, 0){
  POINTS points = MAKEPOINTS(point);
  set_x(points.x);
  set_y(points.y);
}

Point::Point(const POINT& point)
    : PointBase<Point, int, Vector2d>(point.x, point.y) {
}

Point& Point::operator=(const POINT& point) {
  set_x(point.x);
  set_y(point.y);
  return *this;
}

POINT Point::ToPOINT() const {
  POINT p;
  p.x = x();
  p.y = y();
  return p;
}
#elif defined(OS_MACOSX)
Point::Point(const CGPoint& point)
    : PointBase<Point, int, Vector2d>(point.x, point.y) {
}

CGPoint Point::ToCGPoint() const {
  return CGPointMake(x(), y());
}
#endif

std::string Point::ToString() const {
  return base::StringPrintf("%d,%d", x(), y());
}

}  // namespace gfx
