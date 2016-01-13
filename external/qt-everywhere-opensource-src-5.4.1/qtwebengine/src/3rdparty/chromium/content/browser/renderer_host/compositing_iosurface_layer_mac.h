// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"

namespace content {
class CompositingIOSurfaceMac;
class CompositingIOSurfaceContext;
class CompositingIOSurfaceLayerHelper;

class CompositingIOSurfaceLayerClient {
 public:
  virtual void AcceleratedLayerDidDrawFrame(bool succeeded) = 0;
};

}  // namespace content

// The CoreAnimation layer for drawing accelerated content.
@interface CompositingIOSurfaceLayer : CAOpenGLLayer {
 @private
  content::CompositingIOSurfaceLayerClient* client_;
  scoped_refptr<content::CompositingIOSurfaceMac> iosurface_;
  scoped_refptr<content::CompositingIOSurfaceContext> context_;

  // The browser places back-pressure on the GPU by not acknowledging swap
  // calls until they appear on the screen. This can lead to hangs if the
  // view is moved offscreen (among other things). Prevent hangs by always
  // acknowledging the frame after timeout of 1/6th of a second  has passed.
  scoped_ptr<content::CompositingIOSurfaceLayerHelper> helper_;
  scoped_ptr<base::DelayTimer<content::CompositingIOSurfaceLayerHelper>> timer_;

  // Used to track when canDrawInCGLContext should return YES. This can be
  // in response to receiving a new compositor frame, or from any of the events
  // that cause setNeedsDisplay to be called on the layer.
  BOOL needs_display_;

  // This is set when a frame is received, and un-set when the frame is drawn.
  BOOL has_pending_frame_;

  // Incremented every time that this layer is asked to draw but does not have
  // new content to draw.
  uint64 did_not_draw_counter_;
}

- (content::CompositingIOSurfaceMac*)iosurface;
- (content::CompositingIOSurfaceContext*)context;

- (id)initWithIOSurface:(scoped_refptr<content::CompositingIOSurfaceMac>)
                            iosurface
        withScaleFactor:(float)scale_factor
             withClient:(content::CompositingIOSurfaceLayerClient*)client;

// Mark that the client is no longer valid and cannot be called back into.
- (void)resetClient;

// Called when a new frame is received.
- (void)gotNewFrame;

- (void)setNeedsDisplayAndDisplayAndAck;
- (void)displayIfNeededAndAck;
@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_
