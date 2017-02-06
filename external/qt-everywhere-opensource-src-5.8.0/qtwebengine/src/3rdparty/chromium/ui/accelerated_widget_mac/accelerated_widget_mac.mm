// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/accelerated_widget_mac/fullscreen_low_power_coordinator.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/scoped_cgl.h"

@interface CALayer (PrivateAPI)
- (void)setContentsChanged;
@end

namespace ui {
namespace {

typedef std::map<gfx::AcceleratedWidget,AcceleratedWidgetMac*>
    WidgetToHelperMap;
base::LazyInstance<WidgetToHelperMap> g_widget_to_helper_map;


}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMac

AcceleratedWidgetMac::AcceleratedWidgetMac() : view_(nullptr) {
  // Disable the fade-in animation as the layers are added.
  ScopedCAActionDisabler disabler;

  // Add a flipped transparent layer as a child, so that we don't need to
  // fiddle with the position of sub-layers -- they will always be at the
  // origin.
  flipped_layer_.reset([[CALayer alloc] init]);
  [flipped_layer_ setGeometryFlipped:YES];
  [flipped_layer_ setAnchorPoint:CGPointMake(0, 0)];
  [flipped_layer_
      setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];

  fslp_flipped_layer_.reset([[CALayer alloc] init]);
  [fslp_flipped_layer_ setGeometryFlipped:YES];
  [fslp_flipped_layer_ setAnchorPoint:CGPointMake(0, 0)];
  [fslp_flipped_layer_
      setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];

  // Use a sequence number as the accelerated widget handle that we can use
  // to look up the internals structure.
  static intptr_t last_sequence_number = 0;
  last_sequence_number += 1;
  native_widget_ = reinterpret_cast<gfx::AcceleratedWidget>(
      last_sequence_number);
  g_widget_to_helper_map.Pointer()->insert(
      std::make_pair(native_widget_, this));
}

AcceleratedWidgetMac::~AcceleratedWidgetMac() {
  DCHECK(!view_);
  g_widget_to_helper_map.Pointer()->erase(native_widget_);
}

void AcceleratedWidgetMac::SetNSView(AcceleratedWidgetMacNSView* view) {
  // Disable the fade-in animation as the view is added.
  ScopedCAActionDisabler disabler;

  DCHECK(!fslp_coordinator_);
  DCHECK(view && !view_);
  view_ = view;

  CALayer* background_layer = [view_->AcceleratedWidgetGetNSView() layer];
  DCHECK(background_layer);
  [flipped_layer_ setBounds:[background_layer bounds]];
  [fslp_flipped_layer_ setBounds:[background_layer bounds]];
  [background_layer addSublayer:flipped_layer_];
}

void AcceleratedWidgetMac::ResetNSView() {
  if (!view_)
    return;

  if (fslp_coordinator_) {
    fslp_coordinator_->WillLoseAcceleratedWidget();
    DCHECK(!fslp_coordinator_);
  }

  // Disable the fade-out animation as the view is removed.
  ScopedCAActionDisabler disabler;

  [flipped_layer_ removeFromSuperlayer];
  [content_layer_ removeFromSuperlayer];
  [io_surface_layer_ removeFromSuperlayer];
  content_layer_.reset();
  io_surface_layer_.reset();

  last_swap_size_dip_ = gfx::Size();
  view_ = NULL;
}

void AcceleratedWidgetMac::SetFullscreenLowPowerCoordinator(
    FullscreenLowPowerCoordinator* coordinator) {
  DCHECK(coordinator);
  DCHECK(!fslp_coordinator_);
  fslp_coordinator_ = coordinator;
}

void AcceleratedWidgetMac::ResetFullscreenLowPowerCoordinator() {
  DCHECK(fslp_coordinator_);
  fslp_coordinator_ = nullptr;
}

CALayer* AcceleratedWidgetMac::GetFullscreenLowPowerLayer() const {
  return fslp_flipped_layer_;
}

bool AcceleratedWidgetMac::MightBeInFullscreenLowPowerMode() const {
  return fslp_coordinator_;
}

bool AcceleratedWidgetMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  return last_swap_size_dip_ == dip_size;
}

void AcceleratedWidgetMac::GetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) const {
  if (view_) {
    view_->AcceleratedWidgetGetVSyncParameters(timebase, interval);
  } else {
    *timebase = base::TimeTicks();
    *interval = base::TimeDelta();
  }
}

AcceleratedWidgetMac* AcceleratedWidgetMac::Get(gfx::AcceleratedWidget widget) {
  WidgetToHelperMap::const_iterator found =
      g_widget_to_helper_map.Pointer()->find(widget);
  // This can end up being accessed after the underlying widget has been
  // destroyed, but while the ui::Compositor is still being destroyed.
  // Return NULL in these cases.
  if (found == g_widget_to_helper_map.Pointer()->end())
    return nullptr;
  return found->second;
}

void AcceleratedWidgetMac::GotCALayerFrame(
    base::scoped_nsobject<CALayer> content_layer,
    bool fullscreen_low_power_layer_valid,
    base::scoped_nsobject<CALayer> fullscreen_low_power_layer,
    const gfx::Size& pixel_size,
    float scale_factor) {
  TRACE_EVENT0("ui", "AcceleratedWidgetMac::GotCAContextFrame");
  if (!view_) {
    TRACE_EVENT0("ui", "No associated NSView");
    return;
  }
  ScopedCAActionDisabler disabler;
  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);
  if (fullscreen_low_power_layer_valid)
    [fslp_flipped_layer_ setFrame:gfx::Rect(last_swap_size_dip_).ToCGRect()];

  // Ensure that the content is in the CALayer hierarchy, and update fullscreen
  // low power state.
  if (content_layer_ != content_layer) {
    [flipped_layer_ addSublayer:content_layer];
    [content_layer_ removeFromSuperlayer];
    content_layer_ = content_layer;
  }
  if (fullscreen_low_power_layer_ != fullscreen_low_power_layer) {
    if (fslp_coordinator_) {
      fslp_coordinator_->WillLoseAcceleratedWidget();
      DCHECK(!fslp_coordinator_);
    }
    [fslp_flipped_layer_ addSublayer:fullscreen_low_power_layer];
    [fullscreen_low_power_layer_ removeFromSuperlayer];
    fullscreen_low_power_layer_ = fullscreen_low_power_layer;
  }
  if (fslp_coordinator_)
    fslp_coordinator_->SetLowPowerLayerValid(fullscreen_low_power_layer_valid);

  // Ensure the IOSurface is removed.
  if (io_surface_layer_) {
    [io_surface_layer_ removeFromSuperlayer];
    io_surface_layer_.reset();
  }
  view_->AcceleratedWidgetSwapCompleted();
}

void AcceleratedWidgetMac::GotIOSurfaceFrame(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor) {
  TRACE_EVENT0("ui", "AcceleratedWidgetMac::GotIOSurfaceFrame");
  if (!view_) {
    TRACE_EVENT0("ui", "No associated NSView");
    return;
  }
  ScopedCAActionDisabler disabler;
  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

  // Create (if needed) and update the IOSurface layer with new content.
  if (!io_surface_layer_) {
    io_surface_layer_.reset([[CALayer alloc] init]);
    [io_surface_layer_ setContentsGravity:kCAGravityTopLeft];
    [io_surface_layer_ setAnchorPoint:CGPointMake(0, 0)];
    [flipped_layer_ addSublayer:io_surface_layer_];
  }
  id new_contents = (__bridge id)(io_surface.get());
  if (new_contents && new_contents == [io_surface_layer_ contents])
    [io_surface_layer_ setContentsChanged];
  else
    [io_surface_layer_ setContents:new_contents];
  [io_surface_layer_ setBounds:CGRectMake(0, 0, last_swap_size_dip_.width(),
                                          last_swap_size_dip_.height())];
  if ([io_surface_layer_ contentsScale] != scale_factor)
    [io_surface_layer_ setContentsScale:scale_factor];

  // Ensure that the content layer is removed.
  if (content_layer_) {
    [content_layer_ removeFromSuperlayer];
    content_layer_.reset();
  }
  view_->AcceleratedWidgetSwapCompleted();
}

}  // namespace ui
