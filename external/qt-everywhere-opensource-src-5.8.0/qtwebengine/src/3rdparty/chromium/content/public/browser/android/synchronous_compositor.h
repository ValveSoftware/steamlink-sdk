// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;

namespace cc {
class CompositorFrame;
class CompositorFrameAck;
}

namespace gfx {
class Point;
class ScrollOffset;
class Transform;
};

namespace content {

class SynchronousCompositorClient;
class WebContents;

// Interface for embedders that wish to direct compositing operations
// synchronously under their own control. Only meaningful when the
// kEnableSyncrhonousRendererCompositor flag is specified.
class CONTENT_EXPORT SynchronousCompositor {
 public:
  // Must be called once per WebContents instance. Will create the compositor
  // instance as needed, but only if |client| is non-nullptr.
  static void SetClientForWebContents(WebContents* contents,
                                      SynchronousCompositorClient* client);

  struct Frame {
    Frame();
    ~Frame();

    // Movable type.
    Frame(Frame&& rhs);
    Frame& operator=(Frame&& rhs);

    uint32_t output_surface_id;
    std::unique_ptr<cc::CompositorFrame> frame;

   private:
    DISALLOW_COPY_AND_ASSIGN(Frame);
  };

  // "On demand" hardware draw. The content is first clipped to |damage_area|,
  // then transformed through |transform|, and finally clipped to |view_size|.
  virtual Frame DemandDrawHw(
      const gfx::Size& surface_size,
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) = 0;

  // For delegated rendering, return resources from parent compositor to this.
  // Note that all resources must be returned before ReleaseHwDraw.
  virtual void ReturnResources(uint32_t output_surface_id,
                               const cc::CompositorFrameAck& frame_ack) = 0;

  // "On demand" SW draw, into the supplied canvas (observing the transform
  // and clip set there-in).
  virtual bool DemandDrawSw(SkCanvas* canvas) = 0;

  // Set the memory limit policy of this compositor.
  virtual void SetMemoryPolicy(size_t bytes_limit) = 0;

  // Should be called by the embedder after the embedder had modified the
  // scroll offset of the root layer.
  virtual void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) = 0;

  // Allows embedder to synchronously update the zoom level, ie page scale
  // factor, around the anchor point.
  virtual void SynchronouslyZoomBy(float zoom_delta,
                                   const gfx::Point& anchor) = 0;

  // Called by the embedder to notify that the OnComputeScroll step is happening
  // and if any input animation is active, it should tick now.
  virtual void OnComputeScroll(base::TimeTicks animation_time) = 0;

 protected:
  virtual ~SynchronousCompositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_
