// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/public/web/WindowFeaturesStructTraits.h"

namespace mojo {

// static
bool StructTraits<::blink::mojom::WindowFeaturesDataView,
                  ::blink::WebWindowFeatures>::
    Read(::blink::mojom::WindowFeaturesDataView data,
         ::blink::WebWindowFeatures* out) {
  out->x = data.x();
  out->xSet = data.has_x();
  out->y = data.y();
  out->ySet = data.has_y();
  out->width = data.width();
  out->widthSet = data.has_width();
  out->height = data.height();
  out->heightSet = data.has_height();
  out->menuBarVisible = data.menu_bar_visible();
  out->statusBarVisible = data.status_bar_visible();
  out->toolBarVisible = data.tool_bar_visible();
  out->locationBarVisible = data.location_bar_visible();
  out->scrollbarsVisible = data.scrollbars_visible();
  out->resizable = data.resizable();
  out->fullscreen = data.fullscreen();
  out->dialog = data.dialog();
  return true;
}

}  // namespace mojo
