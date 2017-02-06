// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_COMPOSITOR_FRAME_METADATA_STRUCT_TRAITS_H_
#define CC_IPC_COMPOSITOR_FRAME_METADATA_STRUCT_TRAITS_H_

#include "cc/ipc/compositor_frame_metadata.mojom.h"
#include "cc/output/compositor_frame_metadata.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::CompositorFrameMetadata,
                    cc::CompositorFrameMetadata> {
  static float device_scale_factor(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.device_scale_factor;
  }

  static gfx::Vector2dF root_scroll_offset(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.root_scroll_offset;
  }

  static float page_scale_factor(const cc::CompositorFrameMetadata& metadata) {
    return metadata.page_scale_factor;
  }

  static gfx::SizeF scrollable_viewport_size(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.scrollable_viewport_size;
  }

  static gfx::SizeF root_layer_size(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.root_layer_size;
  }

  static float min_page_scale_factor(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.min_page_scale_factor;
  }

  static float max_page_scale_factor(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.max_page_scale_factor;
  }

  static bool root_overflow_x_hidden(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.root_overflow_x_hidden;
  }

  static bool root_overflow_y_hidden(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.root_overflow_y_hidden;
  }

  static gfx::Vector2dF location_bar_offset(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.location_bar_offset;
  }

  static gfx::Vector2dF location_bar_content_translation(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.location_bar_content_translation;
  }

  static uint32_t root_background_color(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.root_background_color;
  }

  static const cc::Selection<gfx::SelectionBound>& selection(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.selection;
  }

  static const std::vector<ui::LatencyInfo>& latency_info(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.latency_info;
  }

  static const std::vector<uint32_t>& satisfies_sequences(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.satisfies_sequences;
  }

  static const std::vector<cc::SurfaceId>& referenced_surfaces(
      const cc::CompositorFrameMetadata& metadata) {
    return metadata.referenced_surfaces;
  }

  static bool Read(cc::mojom::CompositorFrameMetadataDataView data,
                   cc::CompositorFrameMetadata* out);
};

}  // namespace mojo

#endif  // CC_IPC_COMPOSITOR_FRAME_METADATA_STRUCT_TRAITS_H_
