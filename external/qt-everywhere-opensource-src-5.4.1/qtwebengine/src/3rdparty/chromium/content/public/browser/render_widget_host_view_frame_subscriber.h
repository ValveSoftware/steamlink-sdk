// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_FRAME_SUBSCRIBER_H_
#define CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_FRAME_SUBSCRIBER_H_

#include "base/callback.h"
#include "base/time/time.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

// Defines an interface for listening to events of frame presentation and to
// instruct the platform layer (i.e. RenderWidgetHostView) to copy a frame.
//
// Further processing is possible (e.g. scale and color space conversion)
// through this interface. See ShouldCaptureFrame() for details.
//
// It is platform dependent which thread this object lives on, but it is
// guaranteed to be used on a single thread.
class RenderWidgetHostViewFrameSubscriber {
 public:
  virtual ~RenderWidgetHostViewFrameSubscriber() {}

  // Called when a captured frame is available or the frame is no longer
  // needed by the platform layer.
  //
  // If |frame_captured| is true then frame provided contains valid content and
  // |timestamp| is the time when the frame was painted.
  //
  // If |frame_captured| is false then the content in frame provided is
  // invalid. There was an error during the process of frame capture or the
  // platform layer is shutting down. |timestamp| is also invalid in this case.
  typedef base::Callback<void(base::TimeTicks /* timestamp */,
                              bool /* frame_captured */)> DeliverFrameCallback;

  // Called when a new frame is going to be presented at time
  // |present_time|. Implementation can decide whether the current frame should
  // be captured or not.
  //
  // Return true if the current frame should be captured. If so, |storage|
  // should will be set to hold an appropriately sized and allocated buffer
  // into which to copy the frame. The platform presenter will perform scaling
  // and color space conversion to fit into the output frame.
  //
  // Destination format is determined by |storage|, currently only
  // media::VideoFrame::YV12 is supported. Platform layer will perform color
  // space conversion if needed.
  //
  // When the frame is available |callback| will be called. It is up to the
  // platform layer to decide when to deliver a captured frame.
  //
  // Return false if the current frame should not be captured.
  virtual bool ShouldCaptureFrame(base::TimeTicks present_time,
                                  scoped_refptr<media::VideoFrame>* storage,
                                  DeliverFrameCallback* callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_FRAME_SUBSCRIBER_H_
