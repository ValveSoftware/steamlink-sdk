// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A template for a simple rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef UI_GFX_GEOMETRY_RECT_BASE_H_
#define UI_GFX_GEOMETRY_RECT_BASE_H_

#include <string>

#include "base/compiler_specific.h"

namespace gfx {

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
class GFX_EXPORT RectBase {
 public:
  Type x() const { return origin_.x(); }
  void set_x(Type x) { origin_.set_x(x); }

  Type y() const { return origin_.y(); }
  void set_y(Type y) { origin_.set_y(y); }

  Type width() const { return size_.width(); }
  void set_width(Type width) { size_.set_width(width); }

  Type height() const { return size_.height(); }
  void set_height(Type height) { size_.set_height(height); }

  const PointClass& origin() const { return origin_; }
  void set_origin(const PointClass& origin) { origin_ = origin; }

  const SizeClass& size() const { return size_; }
  void set_size(const SizeClass& size) { size_ = size; }

  Type right() const { return x() + width(); }
  Type bottom() const { return y() + height(); }

  PointClass top_right() const { return PointClass(right(), y()); }
  PointClass bottom_left() const { return PointClass(x(), bottom()); }
  PointClass bottom_right() const { return PointClass(right(), bottom()); }

  VectorClass OffsetFromOrigin() const {
    return VectorClass(x(), y());
  }

  void SetRect(Type x, Type y, Type width, Type height);

  // Shrink the rectangle by a horizontal and vertical distance on all sides.
  void Inset(Type horizontal, Type vertical) {
    Inset(horizontal, vertical, horizontal, vertical);
  }

  // Shrink the rectangle by the given insets.
  void Inset(const InsetsClass& insets);

  // Shrink the rectangle by the specified amount on each side.
  void Inset(Type left, Type top, Type right, Type bottom);

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(Type horizontal, Type vertical);
  void Offset(const VectorClass& distance) {
    Offset(distance.x(), distance.y());
  }
  void operator+=(const VectorClass& offset);
  void operator-=(const VectorClass& offset);

  InsetsClass InsetsFrom(const Class& inner) const {
    return InsetsClass(inner.y() - y(),
                       inner.x() - x(),
                       bottom() - inner.bottom(),
                       right() - inner.right());
  }

  // Returns true if the area of the rectangle is zero.
  bool IsEmpty() const { return size_.IsEmpty(); }

  // A rect is less than another rect if its origin is less than
  // the other rect's origin. If the origins are equal, then the
  // shortest rect is less than the other. If the origin and the
  // height are equal, then the narrowest rect is less than.
  // This comparison is required to use Rects in sets, or sorted
  // vectors.
  bool operator<(const Class& other) const;

  // Returns true if the point identified by point_x and point_y falls inside
  // this rectangle.  The point (x, y) is inside the rectangle, but the
  // point (x + width, y + height) is not.
  bool Contains(Type point_x, Type point_y) const;

  // Returns true if the specified point is contained by this rectangle.
  bool Contains(const PointClass& point) const {
    return Contains(point.x(), point.y());
  }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const Class& rect) const;

  // Returns true if this rectangle intersects the specified rectangle.
  // An empty rectangle doesn't intersect any rectangle.
  bool Intersects(const Class& rect) const;

  // Computes the intersection of this rectangle with the given rectangle.
  void Intersect(const Class& rect);

  // Computes the union of this rectangle with the given rectangle.  The union
  // is the smallest rectangle containing both rectangles.
  void Union(const Class& rect);

  // Computes the rectangle resulting from subtracting |rect| from |*this|,
  // i.e. the bounding rect of |Region(*this) - Region(rect)|.
  void Subtract(const Class& rect);

  // Fits as much of the receiving rectangle into the supplied rectangle as
  // possible, becoming the result. For example, if the receiver had
  // a x-location of 2 and a width of 4, and the supplied rectangle had
  // an x-location of 0 with a width of 5, the returned rectangle would have
  // an x-location of 1 with a width of 4.
  void AdjustToFit(const Class& rect);

  // Returns the center of this rectangle.
  PointClass CenterPoint() const;

  // Becomes a rectangle that has the same center point but with a size capped
  // at given |size|.
  void ClampToCenteredSize(const SizeClass& size);

  // Splits |this| in two halves, |left_half| and |right_half|.
  void SplitVertically(Class* left_half, Class* right_half) const;

  // Returns true if this rectangle shares an entire edge (i.e., same width or
  // same height) with the given rectangle, and the rectangles do not overlap.
  bool SharesEdgeWith(const Class& rect) const;

  // Returns the manhattan distance from the rect to the point. If the point is
  // inside the rect, returns 0.
  Type ManhattanDistanceToPoint(const PointClass& point) const;

  // Returns the manhattan distance between the contents of this rect and the
  // contents of the given rect. That is, if the intersection of the two rects
  // is non-empty then the function returns 0. If the rects share a side, it
  // returns the smallest non-zero value appropriate for Type.
  Type ManhattanInternalDistance(const Class& rect) const;

 protected:
  RectBase(const PointClass& origin, const SizeClass& size)
      : origin_(origin), size_(size) {}
  explicit RectBase(const SizeClass& size)
      : size_(size) {}
  explicit RectBase(const PointClass& origin)
      : origin_(origin) {}
  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~RectBase() {}

 private:
  PointClass origin_;
  SizeClass size_;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RECT_BASE_H_
