// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/rect_base.h"

// This file provides the implementation for RectBaese template and
// used to instantiate the base class for Rect and RectF classes.
#if !defined(GFX_IMPLEMENTATION)
#error "This file is intended for UI implementation only"
#endif

namespace {

template<typename Type>
void AdjustAlongAxis(Type dst_origin, Type dst_size, Type* origin, Type* size) {
  *size = std::min(dst_size, *size);
  if (*origin < dst_origin)
    *origin = dst_origin;
  else
    *origin = std::min(dst_origin + dst_size, *origin + *size) - *size;
}

} // namespace

namespace gfx {

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    SetRect(Type x, Type y, Type width, Type height) {
  origin_.SetPoint(x, y);
  set_width(width);
  set_height(height);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Inset(const InsetsClass& insets) {
  Inset(insets.left(), insets.top(), insets.right(), insets.bottom());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Inset(Type left, Type top, Type right, Type bottom) {
  origin_ += VectorClass(left, top);
  set_width(std::max(width() - left - right, static_cast<Type>(0)));
  set_height(std::max(height() - top - bottom, static_cast<Type>(0)));
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Offset(Type horizontal, Type vertical) {
  origin_ += VectorClass(horizontal, vertical);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    operator+=(const VectorClass& offset) {
  origin_ += offset;
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    operator-=(const VectorClass& offset) {
  origin_ -= offset;
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    operator<(const Class& other) const {
  if (origin_ == other.origin_) {
    if (width() == other.width()) {
      return height() < other.height();
    } else {
      return width() < other.width();
    }
  } else {
    return origin_ < other.origin_;
  }
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Contains(Type point_x, Type point_y) const {
  return (point_x >= x()) && (point_x < right()) &&
         (point_y >= y()) && (point_y < bottom());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Contains(const Class& rect) const {
  return (rect.x() >= x() && rect.right() <= right() &&
          rect.y() >= y() && rect.bottom() <= bottom());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Intersects(const Class& rect) const {
  return !(IsEmpty() || rect.IsEmpty() ||
           rect.x() >= right() || rect.right() <= x() ||
           rect.y() >= bottom() || rect.bottom() <= y());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Intersect(const Class& rect) {
  if (IsEmpty() || rect.IsEmpty()) {
    SetRect(0, 0, 0, 0);
    return;
  }

  Type rx = std::max(x(), rect.x());
  Type ry = std::max(y(), rect.y());
  Type rr = std::min(right(), rect.right());
  Type rb = std::min(bottom(), rect.bottom());

  if (rx >= rr || ry >= rb)
    rx = ry = rr = rb = 0;  // non-intersecting

  SetRect(rx, ry, rr - rx, rb - ry);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Union(const Class& rect) {
  if (IsEmpty()) {
    *this = rect;
    return;
  }
  if (rect.IsEmpty())
    return;

  Type rx = std::min(x(), rect.x());
  Type ry = std::min(y(), rect.y());
  Type rr = std::max(right(), rect.right());
  Type rb = std::max(bottom(), rect.bottom());

  SetRect(rx, ry, rr - rx, rb - ry);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    Subtract(const Class& rect) {
  if (!Intersects(rect))
    return;
  if (rect.Contains(*static_cast<const Class*>(this))) {
    SetRect(0, 0, 0, 0);
    return;
  }

  Type rx = x();
  Type ry = y();
  Type rr = right();
  Type rb = bottom();

  if (rect.y() <= y() && rect.bottom() >= bottom()) {
    // complete intersection in the y-direction
    if (rect.x() <= x()) {
      rx = rect.right();
    } else if (rect.right() >= right()) {
      rr = rect.x();
    }
  } else if (rect.x() <= x() && rect.right() >= right()) {
    // complete intersection in the x-direction
    if (rect.y() <= y()) {
      ry = rect.bottom();
    } else if (rect.bottom() >= bottom()) {
      rb = rect.y();
    }
  }
  SetRect(rx, ry, rr - rx, rb - ry);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    AdjustToFit(const Class& rect) {
  Type new_x = x();
  Type new_y = y();
  Type new_width = width();
  Type new_height = height();
  AdjustAlongAxis(rect.x(), rect.width(), &new_x, &new_width);
  AdjustAlongAxis(rect.y(), rect.height(), &new_y, &new_height);
  SetRect(new_x, new_y, new_width, new_height);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
PointClass RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass,
    Type>::CenterPoint() const {
  return PointClass(x() + width() / 2, y() + height() / 2);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    ClampToCenteredSize(const SizeClass& size) {
  Type new_width = std::min(width(), size.width());
  Type new_height = std::min(height(), size.height());
  Type new_x = x() + (width() - new_width) / 2;
  Type new_y = y() + (height() - new_height) / 2;
  SetRect(new_x, new_y, new_width, new_height);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    SplitVertically(Class* left_half, Class* right_half) const {
  DCHECK(left_half);
  DCHECK(right_half);

  left_half->SetRect(x(), y(), width() / 2, height());
  right_half->SetRect(left_half->right(),
                      y(),
                      width() - left_half->width(),
                      height());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    SharesEdgeWith(const Class& rect) const {
  return (y() == rect.y() && height() == rect.height() &&
             (x() == rect.right() || right() == rect.x())) ||
         (x() == rect.x() && width() == rect.width() &&
             (y() == rect.bottom() || bottom() == rect.y()));
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
Type RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    ManhattanDistanceToPoint(const PointClass& point) const {
  Type x_distance = std::max<Type>(0, std::max(
      x() - point.x(), point.x() - right()));
  Type y_distance = std::max<Type>(0, std::max(
      y() - point.y(), point.y() - bottom()));

  return x_distance + y_distance;
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename VectorClass,
         typename Type>
Type RectBase<Class, PointClass, SizeClass, InsetsClass, VectorClass, Type>::
    ManhattanInternalDistance(const Class& rect) const {
  Class c(x(), y(), width(), height());
  c.Union(rect);

  static const Type kEpsilon = std::numeric_limits<Type>::is_integer
                                   ? 1
                                   : std::numeric_limits<Type>::epsilon();

  Type x = std::max<Type>(0, c.width() - width() - rect.width() + kEpsilon);
  Type y = std::max<Type>(0, c.height() - height() - rect.height() + kEpsilon);
  return x + y;
}

}  // namespace gfx
