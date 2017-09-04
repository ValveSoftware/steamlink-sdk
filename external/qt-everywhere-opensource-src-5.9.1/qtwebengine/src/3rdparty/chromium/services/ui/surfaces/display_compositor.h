// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_
#define SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_observer.h"

namespace cc {
class SurfaceHittest;
class SurfaceManager;
}  // namespace cc

namespace ui {

class DisplayCompositorClient;

// The DisplayCompositor object is an object global to the Window Server app
// that holds the SurfaceServer and allocates new Surfaces namespaces.
// This object lives on the main thread of the Window Server.
// TODO(rjkroege, fsamuel): This object will need to change to support multiple
// displays.
class DisplayCompositor : public cc::SurfaceObserver,
                          public base::RefCounted<DisplayCompositor> {
 public:
  explicit DisplayCompositor(cc::mojom::DisplayCompositorClientPtr client);

  // TODO(fsamuel):  These methods should be behind a mojo interface.
  void AddSurfaceReference(const cc::SurfaceId& surface_id,
                           const cc::SurfaceSequence& surface_sequence);
  void ReturnSurfaceReferences(const cc::FrameSinkId& frame_sink_id,
                               const std::vector<uint32_t>& sequences);

  cc::SurfaceManager* manager() { return &manager_; }

 private:
  friend class base::RefCounted<DisplayCompositor>;
  virtual ~DisplayCompositor();

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const cc::SurfaceId& surface_id,
                        const gfx::Size& frame_size,
                        float device_scale_factor) override;
  void OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                        bool* changed) override;

  cc::mojom::DisplayCompositorClientPtr client_;
  cc::SurfaceManager manager_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCompositor);
};

}  // namespace ui

#endif  //  SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_
