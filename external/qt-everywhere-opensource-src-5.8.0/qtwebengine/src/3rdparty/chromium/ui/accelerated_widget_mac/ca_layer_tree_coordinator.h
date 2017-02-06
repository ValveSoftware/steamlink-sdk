// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_HOST_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_HOST_H_

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"
#include "ui/accelerated_widget_mac/gl_renderer_layer_tree.h"

namespace ui {

// A structure that holds the tree of CALayers to display composited content.
// The CALayer tree may consist of a GLRendererLayerTree if the OpenGL renderer
// is being used, or a CARendererLayerTree if the CoreAnimation renderer is
// being used.
//
// This is instantiated in the GPU process and sent to the browser process via
// the cross-process CoreAnimation API. This is intended to be moved entirely
// to the browser process in https://crbug.com/604052.
class ACCELERATED_WIDGET_MAC_EXPORT CALayerTreeCoordinator {
 public:
  explicit CALayerTreeCoordinator(bool allow_remote_layers);
  ~CALayerTreeCoordinator();

  // Set the composited frame's size.
  void Resize(const gfx::Size& pixel_size, float scale_factor);

  // Set the OpenGL backbuffer to which the pending frame was rendered. This is
  // used to draw frames created by the OpenGL renderer.
  bool SetPendingGLRendererBackbuffer(
      base::ScopedCFTypeRef<IOSurfaceRef> backbuffer);

  // The CARendererLayerTree for the pending frame. This is used to construct
  // the CALayer tree for the CoreAnimation renderer.
  CARendererLayerTree* GetPendingCARendererLayerTree();

  // Commit the pending frame's OpenGL backbuffer or CALayer tree to be
  // attached to the root CALayer. If the frame can be displayed using the
  // fullscreen low power layer then |fullscreen_low_power_layer_valid| will
  // be set to true.
  void CommitPendingTreesToCA(const gfx::Rect& pixel_damage_rect,
                              bool* fullscreen_low_power_layer_valid);

  // Get the root CALayer to display the current frame. This does not change
  // over the lifetime of the object.
  CALayer* GetCALayerForDisplay() const;

  // Get the CALayer to display fullscreen low power content. This does not
  // change over the lifetime of the object.
  CALayer* GetFullscreenLowPowerLayerForDisplay() const;

  // Get the current frame's OpenGL backbuffer IOSurface. This is only needed
  // when not using remote layers.
  IOSurfaceRef GetIOSurfaceForDisplay();

 private:
  bool allow_remote_layers_ = true;
  gfx::Size pixel_size_;
  float scale_factor_ = 1;

  base::scoped_nsobject<CALayer> root_ca_layer_;
  base::scoped_nsobject<AVSampleBufferDisplayLayer> fullscreen_low_power_layer_;

  // Frame that has been scheduled, but has not had a subsequent commit call
  // made yet.
  std::unique_ptr<GLRendererLayerTree> pending_gl_renderer_layer_tree_;
  std::unique_ptr<CARendererLayerTree> pending_ca_renderer_layer_tree_;

  // Frame that is currently being displayed on the screen.
  std::unique_ptr<GLRendererLayerTree> current_gl_renderer_layer_tree_;
  std::unique_ptr<CARendererLayerTree> current_ca_renderer_layer_tree_;
  bool current_fullscreen_low_power_layer_valid_ = false;

  DISALLOW_COPY_AND_ASSIGN(CALayerTreeCoordinator);
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_HOST_H_
