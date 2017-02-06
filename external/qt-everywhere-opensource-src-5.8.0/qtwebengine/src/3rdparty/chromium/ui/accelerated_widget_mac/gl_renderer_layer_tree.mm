// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/gl_renderer_layer_tree.h"

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/transform.h"

@interface CALayer(Private)
-(void)setContentsChanged;
@end

namespace ui {
namespace {

// When selecting a CALayer to re-use for partial damage, this is the maximum
// fraction of the merged layer's pixels that may be not-updated by the swap
// before we consider the CALayer to not be a good enough match, and create a
// new one.
const float kMaximumPartialDamageWasteFraction = 1.2f;

// The maximum number of partial damage layers that may be created before we
// give up and remove them all (doing full damage in the process).
const size_t kMaximumPartialDamageLayers = 8;

}  // namespace

class GLRendererLayerTree::OverlayPlane {
 public:
  OverlayPlane(base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
               const gfx::Rect& pixel_frame_rect,
               const gfx::RectF& contents_rect)
      : io_surface(io_surface),
        contents_rect(contents_rect),
        pixel_frame_rect(pixel_frame_rect),
        layer_needs_update(true) {}

  ~OverlayPlane() {
    [ca_layer setContents:nil];
    [ca_layer removeFromSuperlayer];
    ca_layer.reset();
  }

  const base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  const gfx::RectF contents_rect;
  const gfx::Rect pixel_frame_rect;
  bool layer_needs_update;
  base::scoped_nsobject<CALayer> ca_layer;

  void TakeCALayerFrom(OverlayPlane* other_plane) {
    ca_layer.swap(other_plane->ca_layer);
  }

  void UpdateProperties(float scale_factor) {
    if (layer_needs_update) {
      [ca_layer setOpaque:YES];

      id new_contents = (__bridge id)(io_surface.get());
      if ([ca_layer contents] == new_contents)
        [ca_layer setContentsChanged];
      else
        [ca_layer setContents:new_contents];
      [ca_layer setContentsRect:contents_rect.ToCGRect()];

      [ca_layer setAnchorPoint:CGPointZero];

      if ([ca_layer respondsToSelector:(@selector(setContentsScale:))])
        [ca_layer setContentsScale:scale_factor];
      gfx::RectF dip_frame_rect = gfx::RectF(pixel_frame_rect);
      dip_frame_rect.Scale(1 / scale_factor);
      [ca_layer setBounds:CGRectMake(0, 0, dip_frame_rect.width(),
                                     dip_frame_rect.height())];
      [ca_layer
          setPosition:CGPointMake(dip_frame_rect.x(), dip_frame_rect.y())];
    }
    static bool show_borders =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowMacOverlayBorders);
    if (show_borders) {
      base::ScopedCFTypeRef<CGColorRef> color;
      if (!layer_needs_update) {
        // Green represents contents that are unchanged across frames.
        color.reset(CGColorCreateGenericRGB(0, 1, 0, 1));
      } else {
        // Red represents damaged contents.
        color.reset(CGColorCreateGenericRGB(1, 0, 0, 1));
      }
      [ca_layer setBorderWidth:1];
      [ca_layer setBorderColor:color];
    }
    layer_needs_update = false;
  }

 private:
};

void GLRendererLayerTree::UpdatePartialDamagePlanes(
    GLRendererLayerTree* old_tree,
    const gfx::Rect& pixel_damage_rect) {
  // Don't create partial damage layers if partial swap is disabled.
  if (!allow_partial_swap_)
    return;
  // Only create partial damage layers when building on top of an existing tree.
  if (!old_tree)
    return;
  // If the frame size has changed, discard all of the old partial damage
  // layers.
  if (old_tree->root_plane_->pixel_frame_rect != root_plane_->pixel_frame_rect)
    return;
  // If there is full damage, discard all of the old partial damage layers.
  if (pixel_damage_rect == root_plane_->pixel_frame_rect)
    return;

  // If there is no damage, don't change anything.
  if (pixel_damage_rect.IsEmpty()) {
    std::swap(partial_damage_planes_, old_tree->partial_damage_planes_);
    return;
  }

  // Find the last partial damage plane to re-use the CALayer from. Grow the
  // new rect for this layer to include this damage, and all nearby partial
  // damage layers.
  std::unique_ptr<OverlayPlane> plane_for_swap;
  {
    auto plane_to_reuse_iter = old_tree->partial_damage_planes_.end();
    gfx::Rect plane_to_reuse_enlarged_pixel_damage_rect;

    for (auto old_plane_iter = old_tree->partial_damage_planes_.begin();
         old_plane_iter != old_tree->partial_damage_planes_.end();
         ++old_plane_iter) {
      gfx::Rect enlarged_pixel_damage_rect =
          (*old_plane_iter)->pixel_frame_rect;
      enlarged_pixel_damage_rect.Union(pixel_damage_rect);

      // Compute the fraction of the pixels that would not be updated by this
      // swap. If it is too big, try another layer.
      float waste_fraction = enlarged_pixel_damage_rect.size().GetArea() * 1.f /
                             pixel_damage_rect.size().GetArea();
      if (waste_fraction > kMaximumPartialDamageWasteFraction)
        continue;

      plane_to_reuse_iter = old_plane_iter;
      plane_to_reuse_enlarged_pixel_damage_rect.Union(
          enlarged_pixel_damage_rect);
    }
    if (plane_to_reuse_iter != old_tree->partial_damage_planes_.end()) {
      gfx::RectF enlarged_contents_rect =
          gfx::RectF(plane_to_reuse_enlarged_pixel_damage_rect);
      enlarged_contents_rect.Scale(1. / root_plane_->pixel_frame_rect.width(),
                                   1. / root_plane_->pixel_frame_rect.height());

      plane_for_swap.reset(new OverlayPlane(
          root_plane_->io_surface, plane_to_reuse_enlarged_pixel_damage_rect,
          enlarged_contents_rect));

      plane_for_swap->TakeCALayerFrom((*plane_to_reuse_iter).get());
      if (*plane_to_reuse_iter != old_tree->partial_damage_planes_.back()) {
        CALayer* superlayer = [plane_for_swap->ca_layer superlayer];
        [plane_for_swap->ca_layer removeFromSuperlayer];
        [superlayer addSublayer:plane_for_swap->ca_layer];
      }
    }
  }

  // If we haven't found an appropriate layer to re-use, create a new one, if
  // we haven't already created too many.
  if (!plane_for_swap.get() &&
      old_tree->partial_damage_planes_.size() < kMaximumPartialDamageLayers) {
    gfx::RectF contents_rect = gfx::RectF(pixel_damage_rect);
    contents_rect.Scale(1. / root_plane_->pixel_frame_rect.width(),
                        1. / root_plane_->pixel_frame_rect.height());
    plane_for_swap.reset(new OverlayPlane(root_plane_->io_surface,
                                          pixel_damage_rect, contents_rect));
  }

  // And if we still don't have a layer, do full damage.
  if (!plane_for_swap.get())
    return;

  // Walk all old partial damage planes. Remove anything that is now completely
  // covered, and move everything else into the new |partial_damage_planes_|.
  for (auto& old_plane : old_tree->partial_damage_planes_) {
    if (!old_plane.get())
      continue;
    // Intersect the planes' frames with the new root plane to ensure that
    // they don't get kept alive inappropriately.
    gfx::Rect old_plane_frame_rect = old_plane->pixel_frame_rect;
    old_plane_frame_rect.Intersect(root_plane_->pixel_frame_rect);

    bool old_plane_covered_by_swap = false;
    if (plane_for_swap.get() &&
        plane_for_swap->pixel_frame_rect.Contains(old_plane_frame_rect)) {
      old_plane_covered_by_swap = true;
    }
    if (!old_plane_covered_by_swap) {
      DCHECK(old_plane->ca_layer);
      partial_damage_planes_.push_back(std::move(old_plane));
    }
  }

  partial_damage_planes_.push_back(std::move(plane_for_swap));
}

void GLRendererLayerTree::UpdateRootAndPartialDamagePlanes(
    std::unique_ptr<GLRendererLayerTree> old_tree,
    const gfx::Rect& pixel_damage_rect) {
  // First update the partial damage tree.
  UpdatePartialDamagePlanes(old_tree.get(), pixel_damage_rect);
  if (old_tree) {
    if (partial_damage_planes_.empty()) {
      // If there are no partial damage planes, then we will be updating the
      // root layer. Take the CALayer from the old tree.
      root_plane_->TakeCALayerFrom(old_tree->root_plane_.get());
    } else {
      // If there is a partial damage tree, then just take the old plane
      // from the previous frame, so that there is no update to it.
      root_plane_.swap(old_tree->root_plane_);
    }
  }
}

void GLRendererLayerTree::UpdateCALayers(CALayer* superlayer,
                                         float scale_factor) {
  if (!allow_partial_swap_) {
    DCHECK(partial_damage_planes_.empty());
    return;
  }

  // Allocate and update CALayers for the backbuffer and partial damage layers.
  if (!root_plane_->ca_layer) {
    DCHECK(partial_damage_planes_.empty());
    root_plane_->ca_layer.reset([[CALayer alloc] init]);
    [superlayer setSublayers:nil];
    [superlayer addSublayer:root_plane_->ca_layer];
  }
  // Excessive logging to debug white screens (crbug.com/583805).
  // TODO(ccameron): change this back to a DLOG.
  if ([root_plane_->ca_layer superlayer] != superlayer) {
    LOG(ERROR) << "GLRendererLayerTree root layer not attached to tree.";
  }
  for (auto& plane : partial_damage_planes_) {
    if (!plane->ca_layer) {
      DCHECK(plane == partial_damage_planes_.back());
      plane->ca_layer.reset([[CALayer alloc] init]);
    }
    if (![plane->ca_layer superlayer]) {
      DCHECK(plane == partial_damage_planes_.back());
      [superlayer addSublayer:plane->ca_layer];
    }
  }
  root_plane_->UpdateProperties(scale_factor);
  for (auto& plane : partial_damage_planes_)
    plane->UpdateProperties(scale_factor);
}

GLRendererLayerTree::GLRendererLayerTree(
    bool allow_partial_swap,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Rect& pixel_frame_rect)
    : allow_partial_swap_(allow_partial_swap) {
  root_plane_.reset(
      new OverlayPlane(io_surface, pixel_frame_rect, gfx::RectF(0, 0, 1, 1)));
}

GLRendererLayerTree::~GLRendererLayerTree() {}

base::ScopedCFTypeRef<IOSurfaceRef>
GLRendererLayerTree::RootLayerIOSurface() {
  return root_plane_->io_surface;
}

void GLRendererLayerTree::CommitCALayers(
    CALayer* superlayer,
    std::unique_ptr<GLRendererLayerTree> old_tree,
    float scale_factor,
    const gfx::Rect& pixel_damage_rect) {
  TRACE_EVENT0("gpu", "GLRendererLayerTree::CommitCALayers");
  UpdateRootAndPartialDamagePlanes(std::move(old_tree), pixel_damage_rect);
  UpdateCALayers(superlayer, scale_factor);
}

}  // namespace ui
