// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"

namespace mojo {

// static
PointPtr TypeConverter<PointPtr, gfx::Point>::ConvertFrom(
    const gfx::Point& input) {
  PointPtr point(Point::New());
  point->x = input.x();
  point->y = input.y();
  return point.Pass();
}

// static
gfx::Point TypeConverter<PointPtr, gfx::Point>::ConvertTo(
    const PointPtr& input) {
  if (!input)
    return gfx::Point();
  return gfx::Point(input->x, input->y);
}

// static
SizePtr TypeConverter<SizePtr, gfx::Size>::ConvertFrom(const gfx::Size& input) {
  SizePtr size(Size::New());
  size->width = input.width();
  size->height = input.height();
  return size.Pass();
}

// static
gfx::Size TypeConverter<SizePtr, gfx::Size>::ConvertTo(const SizePtr& input) {
  if (!input)
    return gfx::Size();
  return gfx::Size(input->width, input->height);
}

// static
RectPtr TypeConverter<RectPtr, gfx::Rect>::ConvertFrom(const gfx::Rect& input) {
  RectPtr rect(Rect::New());
  rect->x = input.x();
  rect->y = input.y();
  rect->width = input.width();
  rect->height = input.height();
  return rect.Pass();
}

// static
gfx::Rect TypeConverter<RectPtr, gfx::Rect>::ConvertTo(const RectPtr& input) {
  if (!input)
    return gfx::Rect();
  return gfx::Rect(input->x, input->y, input->width, input->height);
}

}  // namespace mojo
