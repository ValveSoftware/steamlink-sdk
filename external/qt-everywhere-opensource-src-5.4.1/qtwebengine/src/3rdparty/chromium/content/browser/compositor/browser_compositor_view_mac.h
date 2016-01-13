// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>

#include "base/mac/scoped_nsobject.h"
#include "cc/output/software_frame_data.h"
#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"
#include "content/browser/renderer_host/software_layer_mac.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class BrowserCompositorViewMacHelper;
}  // namespace content

// Additions to the NSView interface for compositor frames.
@interface NSView (BrowserCompositorView)
- (void)gotAcceleratedIOSurfaceFrame:(IOSurfaceID)surface_handle
                 withOutputSurfaceID:(int)surface_id
                       withPixelSize:(gfx::Size)pixel_size
                     withScaleFactor:(float)scale_factor;

- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas;
@end  // NSView (BrowserCompositorView)

// NSView drawn by a ui::Compositor. The superview of this view is responsible
// for changing the ui::Compositor SizeAndScale and calling layoutLayers when
// the size of the parent view may change. This interface is patterned after
// the needs of RenderWidgetHostViewCocoa, and could change.
@interface BrowserCompositorViewMac : NSView {
  scoped_ptr<ui::Compositor> compositor_;

  base::scoped_nsobject<CALayer> background_layer_;
  base::scoped_nsobject<CompositingIOSurfaceLayer> accelerated_layer_;
  int accelerated_layer_output_surface_id_;
  base::scoped_nsobject<SoftwareLayer> software_layer_;

  scoped_ptr<content::BrowserCompositorViewMacHelper> helper_;
}

// Initialize to render the content of a specific superview.
- (id)initWithSuperview:(NSView*)view;

// Re-position the layers to the correct place when this view's superview
// changes size, or when the accelerated or software content changes.
- (void)layoutLayers;

// Disallow further access to the client.
- (void)resetClient;

// Access the underlying ui::Compositor for this view.
- (ui::Compositor*)compositor;
@end  // BrowserCompositorViewMac

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_MAC_H_
