// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

namespace mojo {

template <>
class TypeConverter<PointPtr, PP_Point> {
 public:
  static PointPtr ConvertFrom(const PP_Point& input) {
    PointPtr point(Point::New());
    point->x = input.x;
    point->y = input.y;
    return point.Pass();
  }

  static PP_Point ConvertTo(const PointPtr& input) {
    if (!input)
      return PP_MakePoint(0, 0);
    return PP_MakePoint(static_cast<int32_t>(input->x),
                        static_cast<int32_t>(input->y));
  }
};

template <>
class TypeConverter<SizePtr, PP_Size> {
 public:
  static SizePtr ConvertFrom(const PP_Size& input) {
    SizePtr size(Size::New());
    size->width = input.width;
    size->height = input.height;
    return size.Pass();
  }

  static PP_Size ConvertTo(const SizePtr& input) {
    if (!input)
      return PP_MakeSize(0, 0);
    return PP_MakeSize(static_cast<int32_t>(input->width),
                       static_cast<int32_t>(input->height));
  }
};

template <>
class TypeConverter<RectPtr, PP_Rect> {
 public:
  static RectPtr ConvertFrom(const PP_Rect& input) {
    RectPtr rect(Rect::New());
    rect->x = input.point.x;
    rect->y = input.point.y;
    rect->width = input.size.width;
    rect->height = input.size.height;
    return rect.Pass();
  }

  static PP_Rect ConvertTo(const RectPtr& input) {
    if (!input)
      return PP_MakeRectFromXYWH(0, 0, 0, 0);
    return PP_MakeRectFromXYWH(input->x, input->y,
                               input->width, input->height);
  }
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_TYPE_CONVERTERS_H_
