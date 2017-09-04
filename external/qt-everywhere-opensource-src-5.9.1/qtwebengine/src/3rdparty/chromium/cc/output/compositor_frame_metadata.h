// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_METADATA_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_METADATA_H_

#include <stdint.h>

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/input/selection.h"
#include "cc/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"

namespace cc {

class CC_EXPORT CompositorFrameMetadata {
 public:
  CompositorFrameMetadata();
  CompositorFrameMetadata(CompositorFrameMetadata&& other);
  ~CompositorFrameMetadata();

  CompositorFrameMetadata& operator=(CompositorFrameMetadata&& other);

  CompositorFrameMetadata Clone() const;

  // The device scale factor used to generate this compositor frame.
  float device_scale_factor = 0.f;

  // Scroll offset and scale of the root layer. This can be used for tasks
  // like positioning windowed plugins.
  gfx::Vector2dF root_scroll_offset;
  float page_scale_factor = 0.f;

  // These limits can be used together with the scroll/scale fields above to
  // determine if scrolling/scaling in a particular direction is possible.
  gfx::SizeF scrollable_viewport_size;
  gfx::SizeF root_layer_size;
  float min_page_scale_factor = 0.f;
  float max_page_scale_factor = 0.f;
  bool root_overflow_x_hidden = false;
  bool root_overflow_y_hidden = false;
  bool may_contain_video = false;

  // WebView makes quality decisions for rastering resourceless software frames
  // based on information that a scroll or animation is active.
  // TODO(aelias): Remove this and always enable filtering if there aren't apps
  // depending on this anymore.
  bool is_resourceless_software_draw_with_scroll_or_animation = false;

  // Used to position the Android location top bar and page content, whose
  // precise position is computed by the renderer compositor.
  float top_controls_height = 0.f;
  float top_controls_shown_ratio = 0.f;

  // Used to position Android bottom bar, whose position is computed by the
  // renderer compositor.
  float bottom_controls_height = 0.f;
  float bottom_controls_shown_ratio = 0.f;

  // This color is usually obtained from the background color of the <body>
  // element. It can be used for filling in gutter areas around the frame when
  // it's too small to fill the box the parent reserved for it.
  SkColor root_background_color = SK_ColorWHITE;

  // Provides selection region updates relative to the current viewport. If the
  // selection is empty or otherwise unused, the bound types will indicate such.
  Selection<gfx::SelectionBound> selection;

  std::vector<ui::LatencyInfo> latency_info;

  // A set of SurfaceSequences that this frame satisfies (always in the same
  // namespace as the current Surface).
  std::vector<uint32_t> satisfies_sequences;

  // This is the set of Surfaces that are referenced by this frame.
  std::vector<SurfaceId> referenced_surfaces;

  // This is a value that allows the browser to associate compositor frames
  // with the content that they represent -- typically top-level page loads.
  // TODO(kenrb, fsamuel): This should eventually by SurfaceID, when they
  // become available in all renderer processes. See https://crbug.com/695579.
  uint32_t content_source_id = 0;

 private:
  CompositorFrameMetadata(const CompositorFrameMetadata& other);
  CompositorFrameMetadata operator=(const CompositorFrameMetadata&) = delete;
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_METADATA_H_
