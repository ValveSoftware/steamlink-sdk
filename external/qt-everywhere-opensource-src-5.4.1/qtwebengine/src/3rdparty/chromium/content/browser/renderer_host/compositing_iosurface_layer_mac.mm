// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/gl.h>

#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gpu_switching_manager.h"

@interface CompositingIOSurfaceLayer(Private)
- (void)ackPendingFrame:(bool)success;
- (void)timerFired;
@end

namespace content {

// The base::DelayTimer needs a C++ class to operate on, rather than Objective C
// class. This helper class provides a bridge between the two.
class CompositingIOSurfaceLayerHelper {
 public:
  CompositingIOSurfaceLayerHelper(CompositingIOSurfaceLayer* layer)
      : layer_(layer) {}
  void TimerFired() {
    [layer_ timerFired];
  }
 private:
  CompositingIOSurfaceLayer* layer_;
};

}  // namespace content

@implementation CompositingIOSurfaceLayer

- (content::CompositingIOSurfaceMac*)iosurface {
  return iosurface_.get();
}

- (content::CompositingIOSurfaceContext*)context {
  return context_.get();
}

- (id)initWithIOSurface:(scoped_refptr<content::CompositingIOSurfaceMac>)
                            iosurface
        withScaleFactor:(float)scale_factor
             withClient:(content::CompositingIOSurfaceLayerClient*)client {
  if (self = [super init]) {
    iosurface_ = iosurface;
    client_ = client;
    helper_.reset(new content::CompositingIOSurfaceLayerHelper(self));
    timer_.reset(new base::DelayTimer<content::CompositingIOSurfaceLayerHelper>(
        FROM_HERE,
        base::TimeDelta::FromSeconds(1) / 6,
        helper_.get(),
        &content::CompositingIOSurfaceLayerHelper::TimerFired));

    context_ = content::CompositingIOSurfaceContext::Get(
        content::CompositingIOSurfaceContext::kCALayerContextWindowNumber);
    DCHECK(context_);
    needs_display_ = NO;
    has_pending_frame_ = NO;
    did_not_draw_counter_ = 0;

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if ([self respondsToSelector:(@selector(setContentsScale:))]) {
      [self setContentsScale:scale_factor];
    }
  }
  return self;
}

- (void)resetClient {
  // Any acks that were waiting on this layer to draw will not occur, so ack
  // them now to prevent blocking the renderer.
  [self ackPendingFrame:true];
  client_ = NULL;
}

- (void)gotNewFrame {
  has_pending_frame_ = YES;
  timer_->Reset();

  // A trace value of 2 indicates that there is a pending swap ack. See
  // canDrawInCGLContext for other value meanings.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self, 2);

  if (context_ && context_->is_vsync_disabled()) {
    // If vsync is disabled, draw immediately and don't bother trying to use
    // the isAsynchronous property to ensure smooth animation.
    [self setNeedsDisplayAndDisplayAndAck];
  } else {
    needs_display_ = YES;
    if (![self isAsynchronous])
      [self setAsynchronous:YES];
  }
}

// Private methods:

- (void)setNeedsDisplayAndDisplayAndAck {
  // Workaround for crbug.com/395827
  if ([self isAsynchronous])
    [self setAsynchronous:NO];

  [self setNeedsDisplay];
  [self displayIfNeededAndAck];
}

- (void)displayIfNeededAndAck {
  // Workaround for crbug.com/395827
  if ([self isAsynchronous])
    [self setAsynchronous:NO];

  [self displayIfNeeded];

  // Calls to setNeedsDisplay can sometimes be ignored, especially if issued
  // rapidly (e.g, with vsync off). This is unacceptable because the failure
  // to ack a single frame will hang the renderer. Ensure that the renderer
  // not be blocked by lying and claiming that we drew the frame.
  [self ackPendingFrame:true];
}

- (void)ackPendingFrame:(bool)success {
  if (!has_pending_frame_)
    return;

  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self, 0);
  has_pending_frame_ = NO;
  if (client_)
    client_->AcceleratedLayerDidDrawFrame(success);
}

- (void)timerFired {
  if (has_pending_frame_)
    [self setNeedsDisplayAndDisplayAndAck];
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  if (!context_)
    return [super copyCGLPixelFormatForDisplayMask:mask];
  return CGLRetainPixelFormat(CGLGetPixelFormat(context_->cgl_context()));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!context_)
    return [super copyCGLContextForPixelFormat:pixelFormat];
  return CGLRetainContext(context_->cgl_context());
}

- (void)setNeedsDisplay {
  needs_display_ = YES;
  [super setNeedsDisplay];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  // Add an instantaneous blip to the PendingSwapAck state to indicate
  // that CoreAnimation asked if a frame is ready. A blip up to to 3 (usually
  // from 2, indicating that a swap ack is pending) indicates that we requested
  // a draw. A blip up to 1 (usually from 0, indicating there is no pending swap
  // ack) indicates that we did not request a draw. This would be more natural
  // to do with a tracing pseudo-thread
  // http://crbug.com/366300
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self, needs_display_ ? 3 : 1);
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self,
                    has_pending_frame_ ? 2 : 0);

  // If we return NO 30 times in a row, switch to being synchronous to avoid
  // burning CPU cycles on this callback.
  if (needs_display_) {
    did_not_draw_counter_ = 0;
  } else {
    did_not_draw_counter_ += 1;
    if (did_not_draw_counter_ > 30)
      [self setAsynchronous:NO];
  }

  return needs_display_;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "CompositingIOSurfaceLayer::drawInCGLContext");

  if (!iosurface_->HasIOSurface() || context_->cgl_context() != glContext) {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  // The correct viewport to cover the layer will be set up by the caller.
  // Transform this into a window size for DrawIOSurface, where it will be
  // transformed back into this viewport.
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  gfx::Rect window_rect(viewport[0], viewport[1], viewport[2], viewport[3]);
  float window_scale_factor = 1.f;
  if ([self respondsToSelector:(@selector(contentsScale))])
    window_scale_factor = [self contentsScale];
  window_rect = ToNearestRect(
      gfx::ScaleRect(window_rect, 1.f/window_scale_factor));

  bool draw_succeeded = iosurface_->DrawIOSurface(
      context_, window_rect, window_scale_factor);

  [self ackPendingFrame:draw_succeeded];
  needs_display_ = NO;

  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end
