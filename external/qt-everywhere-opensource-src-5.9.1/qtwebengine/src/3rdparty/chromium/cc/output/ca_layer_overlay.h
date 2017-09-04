// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_CA_LAYER_OVERLAY_H_
#define CC_OUTPUT_CA_LAYER_OVERLAY_H_

#include "base/memory/ref_counted.h"
#include "cc/quads/render_pass.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/ca_renderer_layer_params.h"

namespace cc {

class DrawQuad;
class RenderPassDrawQuad;
class ResourceProvider;

// Holds information that is frequently shared between consecutive
// CALayerOverlays.
class CC_EXPORT CALayerOverlaySharedState
    : public base::RefCounted<CALayerOverlaySharedState> {
 public:
  CALayerOverlaySharedState() {}
  // Layers in a non-zero sorting context exist in the same 3D space and should
  // intersect.
  unsigned sorting_context_id = 0;
  // If |is_clipped| is true, then clip to |clip_rect| in the target space.
  bool is_clipped = false;
  gfx::RectF clip_rect;
  // The opacity property for the CAayer.
  float opacity = 1;
  // The transform to apply to the CALayer.
  SkMatrix44 transform = SkMatrix44(SkMatrix44::kIdentity_Constructor);

 private:
  friend class base::RefCounted<CALayerOverlaySharedState>;
  ~CALayerOverlaySharedState() {}
};

// Holds all information necessary to construct a CALayer from a DrawQuad.
class CC_EXPORT CALayerOverlay {
 public:
  CALayerOverlay();
  CALayerOverlay(const CALayerOverlay& other);
  ~CALayerOverlay();

  // State that is frequently shared between consecutive CALayerOverlays.
  scoped_refptr<CALayerOverlaySharedState> shared_state;

  // Texture that corresponds to an IOSurface to set as the content of the
  // CALayer. If this is 0 then the CALayer is a solid color.
  unsigned contents_resource_id = 0;
  // The contents rect property for the CALayer.
  gfx::RectF contents_rect;
  // The bounds for the CALayer in pixels.
  gfx::RectF bounds_rect;
  // The background color property for the CALayer.
  SkColor background_color = SK_ColorTRANSPARENT;
  // The edge anti-aliasing mask property for the CALayer.
  unsigned edge_aa_mask = 0;
  // The minification and magnification filters for the CALayer.
  unsigned filter;
  // If |rpdq| is present, then the renderer must draw the filter effects and
  // copy the result into an IOSurface.
  const RenderPassDrawQuad* rpdq = nullptr;
};

typedef std::vector<CALayerOverlay> CALayerOverlayList;

// Returns true if all quads in the root render pass have been replaced by
// CALayerOverlays.
bool ProcessForCALayerOverlays(ResourceProvider* resource_provider,
                               const gfx::RectF& display_rect,
                               const QuadList& quad_list,
                               CALayerOverlayList* ca_layer_overlays);

}  // namespace cc

#endif  // CC_OUTPUT_CA_LAYER_OVERLAY_H_
