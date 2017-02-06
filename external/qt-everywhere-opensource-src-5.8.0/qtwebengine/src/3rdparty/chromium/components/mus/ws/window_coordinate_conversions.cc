// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_coordinate_conversions.h"

#include "components/mus/ws/server_window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace mus {

namespace ws {

namespace {

gfx::Vector2dF CalculateOffsetToAncestor(const ServerWindow* window,
                                         const ServerWindow* ancestor) {
  DCHECK(ancestor->Contains(window));
  gfx::Vector2d result;
  for (const ServerWindow* v = window; v != ancestor; v = v->parent())
    result += v->bounds().OffsetFromOrigin();
  return gfx::Vector2dF(result.x(), result.y());
}

}  // namespace

gfx::Point ConvertPointBetweenWindows(const ServerWindow* from,
                                      const ServerWindow* to,
                                      const gfx::Point& point) {
  return gfx::ToFlooredPoint(
      ConvertPointFBetweenWindows(from, to, gfx::PointF(point.x(), point.y())));
}

gfx::PointF ConvertPointFBetweenWindows(const ServerWindow* from,
                                        const ServerWindow* to,
                                        const gfx::PointF& point) {
  DCHECK(from);
  DCHECK(to);
  if (from == to)
    return point;

  if (from->Contains(to)) {
    const gfx::Vector2dF offset(CalculateOffsetToAncestor(to, from));
    return point - offset;
  }
  DCHECK(to->Contains(from));
  const gfx::Vector2dF offset(CalculateOffsetToAncestor(from, to));
  return point + offset;
}

gfx::Rect ConvertRectBetweenWindows(const ServerWindow* from,
                                    const ServerWindow* to,
                                    const gfx::Rect& rect) {
  DCHECK(from);
  DCHECK(to);
  if (from == to)
    return rect;

  const gfx::Point top_left(
      ConvertPointBetweenWindows(from, to, rect.origin()));
  const gfx::Point bottom_right(gfx::ToCeiledPoint(ConvertPointFBetweenWindows(
      from, to, gfx::PointF(rect.right(), rect.bottom()))));
  return gfx::Rect(top_left.x(), top_left.y(), bottom_right.x() - top_left.x(),
                   bottom_right.y() - top_left.y());
}

}  // namespace ws

}  // namespace mus
