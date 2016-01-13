// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_POINT_F_H_
#define UI_GFX_GEOMETRY_POINT_F_H_

#include <string>

#include "ui/gfx/geometry/point_base.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// A floating version of gfx::Point.
class GFX_EXPORT PointF : public PointBase<PointF, float, Vector2dF> {
 public:
  PointF() : PointBase<PointF, float, Vector2dF>(0, 0) {}
  PointF(float x, float y) : PointBase<PointF, float, Vector2dF>(x, y) {}
  ~PointF() {}

  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    SetPoint(x() * x_scale, y() * y_scale);
  }

  // Returns a string representation of point.
  std::string ToString() const;
};

inline bool operator==(const PointF& lhs, const PointF& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const PointF& lhs, const PointF& rhs) {
  return !(lhs == rhs);
}

inline PointF operator+(const PointF& lhs, const Vector2dF& rhs) {
  PointF result(lhs);
  result += rhs;
  return result;
}

inline PointF operator-(const PointF& lhs, const Vector2dF& rhs) {
  PointF result(lhs);
  result -= rhs;
  return result;
}

inline Vector2dF operator-(const PointF& lhs, const PointF& rhs) {
  return Vector2dF(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

inline PointF PointAtOffsetFromOrigin(const Vector2dF& offset_from_origin) {
  return PointF(offset_from_origin.x(), offset_from_origin.y());
}

GFX_EXPORT PointF ScalePoint(const PointF& p, float x_scale, float y_scale);

inline PointF ScalePoint(const PointF& p, float scale) {
  return ScalePoint(p, scale, scale);
}

#if !defined(COMPILER_MSVC)
extern template class PointBase<PointF, float, Vector2dF>;
#endif

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_POINT_F_H_
