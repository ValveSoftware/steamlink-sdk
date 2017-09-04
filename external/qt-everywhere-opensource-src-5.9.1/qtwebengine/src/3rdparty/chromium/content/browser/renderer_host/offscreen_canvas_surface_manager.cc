// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_manager.h"

namespace content {

namespace {
base::LazyInstance<OffscreenCanvasSurfaceManager>::Leaky g_manager =
    LAZY_INSTANCE_INITIALIZER;
}

OffscreenCanvasSurfaceManager::OffscreenCanvasSurfaceManager() {
  GetSurfaceManager()->AddObserver(this);
}

OffscreenCanvasSurfaceManager::~OffscreenCanvasSurfaceManager() {
  registered_surface_instances_.clear();
  GetSurfaceManager()->RemoveObserver(this);
}

OffscreenCanvasSurfaceManager* OffscreenCanvasSurfaceManager::GetInstance() {
  return g_manager.Pointer();
}

void OffscreenCanvasSurfaceManager::RegisterOffscreenCanvasSurfaceInstance(
    cc::FrameSinkId frame_sink_id,
    OffscreenCanvasSurfaceImpl* surface_instance) {
  DCHECK(surface_instance);
  DCHECK_EQ(registered_surface_instances_.count(frame_sink_id), 0u);
  registered_surface_instances_[frame_sink_id] = surface_instance;
}

void OffscreenCanvasSurfaceManager::UnregisterOffscreenCanvasSurfaceInstance(
    cc::FrameSinkId frame_sink_id) {
  DCHECK_EQ(registered_surface_instances_.count(frame_sink_id), 1u);
  registered_surface_instances_.erase(frame_sink_id);
}

OffscreenCanvasSurfaceImpl* OffscreenCanvasSurfaceManager::GetSurfaceInstance(
    cc::FrameSinkId frame_sink_id) {
  auto search = registered_surface_instances_.find(frame_sink_id);
  if (search != registered_surface_instances_.end()) {
    return search->second;
  }
  return nullptr;
}

}  // namespace content
