// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryStructTraits_h
#define GeometryStructTraits_h

#include "platform/PlatformExport.h"
#include "ui/gfx/geometry/mojo/geometry.mojom-blink.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT
    StructTraits<gfx::mojom::blink::Size::DataView, ::blink::WebSize> {
  static int width(const ::blink::WebSize& size) { return size.width; }
  static int height(const ::blink::WebSize& size) { return size.height; }
  static bool Read(gfx::mojom::blink::Size::DataView, ::blink::WebSize* out);
};

}  // namespace mojo

#endif  // GeometryStructTraits_h
