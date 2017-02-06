// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RESIZE_PARAMS_H_
#define CONTENT_COMMON_RESIZE_PARAMS_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/gfx/geometry/size.h"

namespace content {

struct CONTENT_EXPORT ResizeParams {
  ResizeParams();
  ResizeParams(const ResizeParams& other);
  ~ResizeParams();

  // Information about the screen (dpi, depth, etc..).
  blink::WebScreenInfo screen_info;

  // The size of the renderer.
  gfx::Size new_size;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  gfx::Size physical_backing_size;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar (always false on platforms where URL-bar hiding isn't supported).
  bool top_controls_shrink_blink_size;

  // The height of the top controls (always 0 on platforms where URL-bar hiding
  // isn't supported).
  float top_controls_height;

  // The size of the visible viewport, which may be smaller than the view if the
  // view is partially occluded (e.g. by a virtual keyboard).  The size is in
  // DPI-adjusted pixels.
  gfx::Size visible_viewport_size;

  // The resizer rect.
  gfx::Rect resizer_rect;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted;

  // The display mode.
  blink::WebDisplayMode display_mode;

  // If set, requests the renderer to reply with a ViewHostMsg_UpdateRect
  // with the ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK bit set in flags.
  bool needs_resize_ack;
};

}  // namespace content

#endif  // CONTENT_COMMON_RESIZE_PARAMS_H_
