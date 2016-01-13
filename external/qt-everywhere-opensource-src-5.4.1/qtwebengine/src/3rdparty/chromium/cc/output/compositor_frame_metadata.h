// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_METADATA_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_METADATA_H_

#include <vector>

#include "cc/base/cc_export.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

class CC_EXPORT CompositorFrameMetadata {
 public:
  CompositorFrameMetadata();
  ~CompositorFrameMetadata();

  // The device scale factor used to generate this compositor frame.
  float device_scale_factor;

  // Scroll offset and scale of the root layer. This can be used for tasks
  // like positioning windowed plugins.
  gfx::Vector2dF root_scroll_offset;
  float page_scale_factor;

  // These limits can be used together with the scroll/scale fields above to
  // determine if scrolling/scaling in a particular direction is possible.
  gfx::SizeF viewport_size;
  gfx::SizeF root_layer_size;
  float min_page_scale_factor;
  float max_page_scale_factor;

  // Used to position the Android location top bar and page content, whose
  // precise position is computed by the renderer compositor.
  gfx::Vector2dF location_bar_offset;
  gfx::Vector2dF location_bar_content_translation;
  float overdraw_bottom_height;

  std::vector<ui::LatencyInfo> latency_info;
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_METADATA_H_
