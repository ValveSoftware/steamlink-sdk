// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/app_struct_traits.h"

namespace mojo {

bool StructTraits<arc::mojom::ScreenRectDataView, gfx::Rect>::Read(
    arc::mojom::ScreenRectDataView data,
    gfx::Rect* out) {
  if (data.right() < data.left() || data.bottom() < data.top())
    return false;

  out->SetRect(data.left(), data.top(), data.right() - data.left(),
               data.bottom() - data.top());
  return true;
}

}  // namespace mojo
