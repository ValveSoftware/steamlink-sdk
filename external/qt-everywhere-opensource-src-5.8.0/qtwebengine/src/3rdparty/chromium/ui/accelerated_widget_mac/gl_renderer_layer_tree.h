// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_LAYER_PARTIAL_DAMAGE_TREE_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_LAYER_PARTIAL_DAMAGE_TREE_MAC_H_

#include <IOSurface/IOSurface.h>
#include <QuartzCore/QuartzCore.h>

#include <deque>
#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

class ACCELERATED_WIDGET_MAC_EXPORT GLRendererLayerTree {
 public:
  GLRendererLayerTree(bool allow_partial_swap,
                      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                      const gfx::Rect& pixel_frame_rect);
  ~GLRendererLayerTree();

  base::ScopedCFTypeRef<IOSurfaceRef> RootLayerIOSurface();
  void CommitCALayers(CALayer* superlayer,
                      std::unique_ptr<GLRendererLayerTree> old_tree,
                      float scale_factor,
                      const gfx::Rect& pixel_damage_rect);

 private:
  class OverlayPlane;

  // This will populate |partial_damage_planes_|, potentially re-using the
  // CALayers and |partial_damage_planes_| from |old_tree|. After this function
  // completes, the back() of |partial_damage_planes_| is the plane that will
  // be updated this frame (and if it is empty, then the root plane will be
  // updated).
  void UpdatePartialDamagePlanes(GLRendererLayerTree* old_tree,
                                 const gfx::Rect& pixel_damage_rect);

  void UpdateRootAndPartialDamagePlanes(
      std::unique_ptr<GLRendererLayerTree> old_tree,
      const gfx::Rect& pixel_damage_rect);

  void UpdateCALayers(CALayer* superlayer, float scale_factor);

  const bool allow_partial_swap_;
  std::unique_ptr<OverlayPlane> root_plane_;
  std::deque<std::unique_ptr<OverlayPlane>> partial_damage_planes_;
};

}  // content

#endif  // GPU_IPC_SERVICE_CA_LAYER_PARTIAL_DAMAGE_TREE_MAC_H_
