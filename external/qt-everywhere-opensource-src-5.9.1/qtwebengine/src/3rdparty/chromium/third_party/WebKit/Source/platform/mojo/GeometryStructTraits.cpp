// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/GeometryStructTraits.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::blink::Size::DataView, ::blink::WebSize>::Read(
    gfx::mojom::blink::Size::DataView data,
    ::blink::WebSize* out) {
  if (data.width() < 0 || data.height() < 0)
    return false;
  out->width = data.width();
  out->height = data.height();
  return true;
}

}  // namespace mojo
