// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/sync_compositor_messages.h"

namespace content {

SyncCompositorDemandDrawHwParams::SyncCompositorDemandDrawHwParams() {}

SyncCompositorDemandDrawHwParams::SyncCompositorDemandDrawHwParams(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority)
    : surface_size(surface_size),
      transform(transform),
      viewport(viewport),
      clip(clip),
      viewport_rect_for_tile_priority(viewport_rect_for_tile_priority),
      transform_for_tile_priority(transform_for_tile_priority) {}

SyncCompositorDemandDrawHwParams::~SyncCompositorDemandDrawHwParams() {}

SyncCompositorSetSharedMemoryParams::SyncCompositorSetSharedMemoryParams()
    : buffer_size(0u) {}

SyncCompositorDemandDrawSwParams::SyncCompositorDemandDrawSwParams() {}

SyncCompositorDemandDrawSwParams::~SyncCompositorDemandDrawSwParams() {}

SyncCompositorCommonRendererParams::SyncCompositorCommonRendererParams()
    : version(0u),
      page_scale_factor(0.f),
      min_page_scale_factor(0.f),
      max_page_scale_factor(0.f),
      need_animate_scroll(false),
      need_invalidate_count(0u),
      did_activate_pending_tree_count(0u) {}

SyncCompositorCommonRendererParams::~SyncCompositorCommonRendererParams() {}

}  // namespace content
