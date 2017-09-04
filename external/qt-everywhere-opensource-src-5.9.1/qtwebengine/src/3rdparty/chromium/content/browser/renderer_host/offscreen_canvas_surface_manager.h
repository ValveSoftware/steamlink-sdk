// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_observer.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

namespace content {

class CONTENT_EXPORT OffscreenCanvasSurfaceManager
    : public cc::SurfaceObserver {
 public:
  OffscreenCanvasSurfaceManager();
  virtual ~OffscreenCanvasSurfaceManager();

  static OffscreenCanvasSurfaceManager* GetInstance();

  void RegisterOffscreenCanvasSurfaceInstance(cc::FrameSinkId,
                                              OffscreenCanvasSurfaceImpl*);
  void UnregisterOffscreenCanvasSurfaceInstance(cc::FrameSinkId);
  OffscreenCanvasSurfaceImpl* GetSurfaceInstance(cc::FrameSinkId);

 private:
  friend class OffscreenCanvasSurfaceManagerTest;

  // cc::SurfaceObserver implementation.
  // TODO(crbug.com/662498): Implement these functions when
  // OffscreenCanvasSurfaceManager must propagate the resizing information
  // back to renderer/main.
  void OnSurfaceCreated(const cc::SurfaceId& surface_id,
                        const gfx::Size& frame_size,
                        float device_scale_factor) override {}
  void OnSurfaceDamaged(const cc::SurfaceId&, bool* changed) override {}

  // When an OffscreenCanvasSurfaceImpl instance is destructed, it will
  // unregister the corresponding entry from this map.
  std::unordered_map<cc::FrameSinkId,
                     OffscreenCanvasSurfaceImpl*,
                     cc::FrameSinkIdHash>
      registered_surface_instances_;
  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_MANAGER_H_
