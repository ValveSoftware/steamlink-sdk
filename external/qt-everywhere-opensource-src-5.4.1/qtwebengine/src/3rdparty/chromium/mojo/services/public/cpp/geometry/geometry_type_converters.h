// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_

#include "mojo/services/public/cpp/geometry/mojo_geometry_export.h"
#include "mojo/services/public/interfaces/geometry/geometry.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

template<>
class MOJO_GEOMETRY_EXPORT TypeConverter<PointPtr, gfx::Point> {
 public:
  static PointPtr ConvertFrom(const gfx::Point& input);
  static gfx::Point ConvertTo(const PointPtr& input);
};

template<>
class MOJO_GEOMETRY_EXPORT TypeConverter<SizePtr, gfx::Size> {
 public:
  static SizePtr ConvertFrom(const gfx::Size& input);
  static gfx::Size ConvertTo(const SizePtr& input);
};

template<>
class MOJO_GEOMETRY_EXPORT TypeConverter<RectPtr, gfx::Rect> {
 public:
  static RectPtr ConvertFrom(const gfx::Rect& input);
  static gfx::Rect ConvertTo(const RectPtr& input);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_GEOMETRY_GEOMETRY_TYPE_CONVERTERS_H_
