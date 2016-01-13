// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_FRAME_COMPOSITOR_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_FRAME_COMPOSITOR_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace media {
class VideoFrame;
}

namespace content {

// VideoFrameCompositor handles incoming frames by notifying the compositor and
// dispatching callbacks when detecting changes in video frames.
//
// Typical usage is to deliver ready-to-be-displayed video frames to
// UpdateCurrentFrame() so that VideoFrameCompositor can take care of tracking
// changes in video frames and firing callbacks as needed.
//
// VideoFrameCompositor must live on the same thread as the compositor.
class CONTENT_EXPORT VideoFrameCompositor
    : NON_EXPORTED_BASE(public cc::VideoFrameProvider) {
 public:
  // |natural_size_changed_cb| is run with the new natural size of the video
  // frame whenever a change in natural size is detected. It is not called the
  // first time UpdateCurrentFrame() is called. Run on the same thread as the
  // caller of UpdateCurrentFrame().
  //
  // |opacity_changed_cb| is run when a change in opacity is detected. It *is*
  // called the first time UpdateCurrentFrame() is called. Run on the same
  // thread as the caller of UpdateCurrentFrame().
  //
  // TODO(scherkus): Investigate the inconsistency between the callbacks with
  // respect to why we don't call |natural_size_changed_cb| on the first frame.
  // I suspect it was for historical reasons that no longer make sense.
  VideoFrameCompositor(
      const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
      const base::Callback<void(bool)>& opacity_changed_cb);
  virtual ~VideoFrameCompositor();

  // cc::VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE;

  // Updates the current frame and notifies the compositor.
  void UpdateCurrentFrame(const scoped_refptr<media::VideoFrame>& frame);

 private:
  base::Callback<void(gfx::Size)> natural_size_changed_cb_;
  base::Callback<void(bool)> opacity_changed_cb_;

  cc::VideoFrameProvider::Client* client_;

  scoped_refptr<media::VideoFrame> current_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_FRAME_COMPOSITOR_H_
