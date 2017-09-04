// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowFeaturesStructTraits_h
#define WindowFeaturesStructTraits_h

#include "WebWindowFeatures.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/WebKit/public/web/window_features.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<::blink::mojom::WindowFeaturesDataView,
                    ::blink::WebWindowFeatures> {
  static float x(const ::blink::WebWindowFeatures& features) {
    return features.x;
  }
  static bool has_x(const ::blink::WebWindowFeatures& features) {
    return features.xSet;
  }
  static float y(const ::blink::WebWindowFeatures& features) {
    return features.y;
  }
  static bool has_y(const ::blink::WebWindowFeatures& features) {
    return features.ySet;
  }
  static float width(const ::blink::WebWindowFeatures& features) {
    return features.width;
  }
  static bool has_width(const ::blink::WebWindowFeatures& features) {
    return features.widthSet;
  }
  static float height(const ::blink::WebWindowFeatures& features) {
    return features.height;
  }
  static bool has_height(const ::blink::WebWindowFeatures& features) {
    return features.heightSet;
  }

  static bool menu_bar_visible(const ::blink::WebWindowFeatures& features) {
    return features.menuBarVisible;
  }
  static bool status_bar_visible(const ::blink::WebWindowFeatures& features) {
    return features.statusBarVisible;
  }
  static bool tool_bar_visible(const ::blink::WebWindowFeatures& features) {
    return features.toolBarVisible;
  }
  static bool location_bar_visible(const ::blink::WebWindowFeatures& features) {
    return features.locationBarVisible;
  }
  static bool scrollbars_visible(const ::blink::WebWindowFeatures& features) {
    return features.scrollbarsVisible;
  }
  static bool resizable(const ::blink::WebWindowFeatures& features) {
    return features.resizable;
  }

  static bool fullscreen(const ::blink::WebWindowFeatures& features) {
    return features.fullscreen;
  }
  static bool dialog(const ::blink::WebWindowFeatures& features) {
    return features.dialog;
  }

  static bool Read(::blink::mojom::WindowFeaturesDataView,
                   ::blink::WebWindowFeatures* out);
};

}  // namespace mojo

#endif
