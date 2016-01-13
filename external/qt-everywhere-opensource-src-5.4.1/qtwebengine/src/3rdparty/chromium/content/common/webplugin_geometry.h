// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEBPLUGIN_GEOMETRY_H_
#define CONTENT_COMMON_WEBPLUGIN_GEOMETRY_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace content {

// Describes the new location for a plugin window.
struct WebPluginGeometry {
  WebPluginGeometry();
  ~WebPluginGeometry();

  bool Equals(const WebPluginGeometry& rhs) const;

  // On Windows, this is the plugin window in the plugin process.
  // On X11, this is the XID of the plugin-side GtkPlug containing the
  // GtkSocket hosting the actual plugin window.
  //
  // On Mac OS X, all of the plugin types are currently "windowless"
  // (window == 0) except for the special case of the GPU plugin,
  // which currently performs rendering on behalf of the Pepper 3D API
  // and WebGL. The GPU plugin uses a simple integer for the
  // PluginWindowHandle which is used to map to a side data structure
  // containing information about the plugin. Soon this plugin will be
  // generalized, at which point this mechanism will be rethought or
  // removed.
  gfx::PluginWindowHandle window;
  gfx::Rect window_rect;
  // Clip rect (include) and cutouts (excludes), relative to
  // window_rect origin.
  gfx::Rect clip_rect;
  std::vector<gfx::Rect> cutout_rects;
  bool rects_valid;
  bool visible;
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBPLUGIN_GEOMETRY_H_
