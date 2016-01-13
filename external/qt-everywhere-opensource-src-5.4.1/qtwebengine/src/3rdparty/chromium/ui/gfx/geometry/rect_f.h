// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_RECT_F_H_
#define UI_GFX_GEOMETRY_RECT_F_H_

#include <string>

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_base.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

class InsetsF;

// A floating version of gfx::Rect.
class GFX_EXPORT RectF
    : public RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float> {
 public:
  RectF()
      : RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>
            (SizeF()) {}

  RectF(float width, float height)
      : RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>
            (SizeF(width, height)) {}

  RectF(float x, float y, float width, float height)
      : RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>
            (PointF(x, y), SizeF(width, height)) {}

  explicit RectF(const SizeF& size)
      : RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>
            (size) {}

  RectF(const PointF& origin, const SizeF& size)
      : RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>
            (origin, size) {}

  ~RectF() {}

  // Scales the rectangle by |scale|.
  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    set_origin(ScalePoint(origin(), x_scale, y_scale));
    set_size(ScaleSize(size(), x_scale, y_scale));
  }

  // This method reports if the RectF can be safely converted to an integer
  // Rect. When it is false, some dimension of the RectF is outside the bounds
  // of what an integer can represent, and converting it to a Rect will require
  // clamping.
  bool IsExpressibleAsRect() const;

  std::string ToString() const;
};

inline bool operator==(const RectF& lhs, const RectF& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const RectF& lhs, const RectF& rhs) {
  return !(lhs == rhs);
}

inline RectF operator+(const RectF& lhs, const Vector2dF& rhs) {
  return RectF(lhs.x() + rhs.x(), lhs.y() + rhs.y(),
      lhs.width(), lhs.height());
}

inline RectF operator-(const RectF& lhs, const Vector2dF& rhs) {
  return RectF(lhs.x() - rhs.x(), lhs.y() - rhs.y(),
      lhs.width(), lhs.height());
}

inline RectF operator+(const Vector2dF& lhs, const RectF& rhs) {
  return rhs + lhs;
}

GFX_EXPORT RectF IntersectRects(const RectF& a, const RectF& b);
GFX_EXPORT RectF UnionRects(const RectF& a, const RectF& b);
GFX_EXPORT RectF SubtractRects(const RectF& a, const RectF& b);

inline RectF ScaleRect(const RectF& r, float x_scale, float y_scale) {
  return RectF(r.x() * x_scale, r.y() * y_scale,
       r.width() * x_scale, r.height() * y_scale);
}

inline RectF ScaleRect(const RectF& r, float scale) {
  return ScaleRect(r, scale, scale);
}

// Constructs a rectangle with |p1| and |p2| as opposite corners.
//
// This could also be thought of as "the smallest rect that contains both
// points", except that we consider points on the right/bottom edges of the
// rect to be outside the rect.  So technically one or both points will not be
// contained within the rect, because they will appear on one of these edges.
GFX_EXPORT RectF BoundingRect(const PointF& p1, const PointF& p2);

#if !defined(COMPILER_MSVC)
extern template class RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>;
#endif

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RECT_F_H_
