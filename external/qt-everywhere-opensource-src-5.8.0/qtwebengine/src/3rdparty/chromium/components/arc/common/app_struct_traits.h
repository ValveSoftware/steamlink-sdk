// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_APP_STRUCT_TRAITS_H_
#define COMPONENTS_ARC_COMMON_APP_STRUCT_TRAITS_H_

#include "components/arc/common/app.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

template <>
struct StructTraits<arc::mojom::ScreenRect, gfx::Rect> {
  static int32_t left(const gfx::Rect& p) { return p.x(); }
  static int32_t top(const gfx::Rect& p) { return p.y(); }
  static int32_t right(const gfx::Rect& p) { return p.right(); }
  static int32_t bottom(const gfx::Rect& p) { return p.bottom(); }
  static bool Read(arc::mojom::ScreenRectDataView data, gfx::Rect* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_COMMON_APP_STRUCT_TRAITS_H_
