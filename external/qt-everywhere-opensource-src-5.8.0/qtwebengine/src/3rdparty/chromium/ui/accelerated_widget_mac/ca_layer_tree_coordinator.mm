// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"

#include <AVFoundation/AVFoundation.h>

#include "base/trace_event/trace_event.h"
#include "ui/base/cocoa/animation_utils.h"

namespace ui {

CALayerTreeCoordinator::CALayerTreeCoordinator(bool allow_remote_layers)
    : allow_remote_layers_(allow_remote_layers) {
  if (allow_remote_layers_) {
    root_ca_layer_.reset([[CALayer alloc] init]);
    [root_ca_layer_ setGeometryFlipped:YES];
    [root_ca_layer_ setOpaque:YES];

    fullscreen_low_power_layer_.reset(
        [[AVSampleBufferDisplayLayer alloc] init]);
  }
}

CALayerTreeCoordinator::~CALayerTreeCoordinator() {}

void CALayerTreeCoordinator::Resize(const gfx::Size& pixel_size,
                                    float scale_factor) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
}

bool CALayerTreeCoordinator::SetPendingGLRendererBackbuffer(
    base::ScopedCFTypeRef<IOSurfaceRef> backbuffer) {
  if (pending_ca_renderer_layer_tree_) {
    DLOG(ERROR) << "Either CALayer overlays or a backbuffer should be "
                   "specified, but not both.";
    return false;
  }
  if (pending_gl_renderer_layer_tree_) {
    DLOG(ERROR) << "Only one backbuffer per swap is allowed.";
    return false;
  }
  pending_gl_renderer_layer_tree_.reset(new GLRendererLayerTree(
      allow_remote_layers_, backbuffer, gfx::Rect(pixel_size_)));

  return true;
}

CARendererLayerTree* CALayerTreeCoordinator::GetPendingCARendererLayerTree() {
  DCHECK(allow_remote_layers_);
  if (pending_gl_renderer_layer_tree_) {
    DLOG(ERROR) << "Either CALayer overlays or a backbuffer should be "
                   "specified, but not both.";
  }
  if (!pending_ca_renderer_layer_tree_)
    pending_ca_renderer_layer_tree_.reset(new CARendererLayerTree);
  return pending_ca_renderer_layer_tree_.get();
}

void CALayerTreeCoordinator::CommitPendingTreesToCA(
    const gfx::Rect& pixel_damage_rect,
    bool* fullscreen_low_power_layer_valid) {
  *fullscreen_low_power_layer_valid = false;

  // Update the CALayer hierarchy.
  ScopedCAActionDisabler disabler;
  if (pending_ca_renderer_layer_tree_) {
    pending_ca_renderer_layer_tree_->CommitScheduledCALayers(
        root_ca_layer_.get(), std::move(current_ca_renderer_layer_tree_),
        scale_factor_);
    *fullscreen_low_power_layer_valid =
        pending_ca_renderer_layer_tree_->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    current_ca_renderer_layer_tree_.swap(pending_ca_renderer_layer_tree_);
    current_gl_renderer_layer_tree_.reset();
  } else if (pending_gl_renderer_layer_tree_) {
    pending_gl_renderer_layer_tree_->CommitCALayers(
        root_ca_layer_.get(), std::move(current_gl_renderer_layer_tree_),
        scale_factor_, pixel_damage_rect);
    current_gl_renderer_layer_tree_.swap(pending_gl_renderer_layer_tree_);
    current_ca_renderer_layer_tree_.reset();
  } else {
    TRACE_EVENT0("gpu", "Blank frame: No overlays or CALayers");
    [root_ca_layer_ setSublayers:nil];
    current_gl_renderer_layer_tree_.reset();
    current_ca_renderer_layer_tree_.reset();
  }

  // TODO(ccameron): It may be necessary to leave the last image up for a few
  // extra frames to allow a smooth switch between the normal and low-power
  // NSWindows.
  if (current_fullscreen_low_power_layer_valid_ &&
      !*fullscreen_low_power_layer_valid) {
    [fullscreen_low_power_layer_ flushAndRemoveImage];
  }
  current_fullscreen_low_power_layer_valid_ = *fullscreen_low_power_layer_valid;

  // Reset all state for the next frame.
  pending_ca_renderer_layer_tree_.reset();
  pending_gl_renderer_layer_tree_.reset();
}

CALayer* CALayerTreeCoordinator::GetCALayerForDisplay() const {
  DCHECK(allow_remote_layers_);
  return root_ca_layer_.get();
}

CALayer* CALayerTreeCoordinator::GetFullscreenLowPowerLayerForDisplay() const {
  return fullscreen_low_power_layer_.get();
}

IOSurfaceRef CALayerTreeCoordinator::GetIOSurfaceForDisplay() {
  DCHECK(!allow_remote_layers_);
  if (!current_gl_renderer_layer_tree_)
    return nullptr;
  return current_gl_renderer_layer_tree_->RootLayerIOSurface();
}

}  // namespace ui
