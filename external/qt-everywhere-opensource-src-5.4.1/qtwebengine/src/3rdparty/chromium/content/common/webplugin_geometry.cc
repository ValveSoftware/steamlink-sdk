// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webplugin_geometry.h"

namespace content {

WebPluginGeometry::WebPluginGeometry()
    : window(gfx::kNullPluginWindow),
      rects_valid(false),
      visible(false) {
}

WebPluginGeometry::~WebPluginGeometry() {
}

bool WebPluginGeometry::Equals(const WebPluginGeometry& rhs) const {
  return window == rhs.window &&
         window_rect == rhs.window_rect &&
         clip_rect == rhs.clip_rect &&
         cutout_rects == rhs.cutout_rects &&
         rects_valid == rhs.rects_valid &&
         visible == rhs.visible;
}

}  // namespace content
