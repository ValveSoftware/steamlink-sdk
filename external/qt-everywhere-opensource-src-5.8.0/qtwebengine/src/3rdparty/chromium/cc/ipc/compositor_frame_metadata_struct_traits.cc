// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/compositor_frame_metadata_struct_traits.h"
#include "cc/ipc/selection_struct_traits.h"
#include "cc/ipc/surface_id_struct_traits.h"
#include "ui/events/mojo/latency_info_struct_traits.h"
#include "ui/gfx/mojo/selection_bound_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::CompositorFrameMetadata,
                  cc::CompositorFrameMetadata>::
    Read(cc::mojom::CompositorFrameMetadataDataView data,
         cc::CompositorFrameMetadata* out) {
  out->device_scale_factor = data.device_scale_factor();
  if (!data.ReadRootScrollOffset(&out->root_scroll_offset))
    return false;

  out->page_scale_factor = data.page_scale_factor();
  if (!data.ReadScrollableViewportSize(&out->scrollable_viewport_size) ||
      !data.ReadRootLayerSize(&out->root_layer_size)) {
    return false;
  }

  out->min_page_scale_factor = data.min_page_scale_factor();
  out->max_page_scale_factor = data.max_page_scale_factor();
  out->root_overflow_x_hidden = data.root_overflow_x_hidden();
  out->root_overflow_y_hidden = data.root_overflow_y_hidden();
  if (!data.ReadLocationBarOffset(&out->location_bar_offset) ||
      !data.ReadLocationBarContentTranslation(
          &out->location_bar_content_translation)) {
    return false;
  }

  out->root_background_color = data.root_background_color();
  return data.ReadSelection(&out->selection) &&
         data.ReadLatencyInfo(&out->latency_info) &&
         data.ReadSatisfiesSequences(&out->satisfies_sequences) &&
         data.ReadReferencedSurfaces(&out->referenced_surfaces);
}

}  // namespace mojo
