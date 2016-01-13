// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SKCANVAS_VIDEO_RENDERER_H_
#define MEDIA_FILTERS_SKCANVAS_VIDEO_RENDERER_H_

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace media {

class VideoFrame;

// Handles rendering of VideoFrames to SkCanvases, doing any necessary YUV
// conversion and caching of resulting RGB bitmaps.
class MEDIA_EXPORT SkCanvasVideoRenderer {
 public:
  SkCanvasVideoRenderer();
  ~SkCanvasVideoRenderer();

  // Paints |video_frame| on |canvas|, scaling the result to fit dimensions
  // specified by |dest_rect|.
  //
  // Black will be painted on |canvas| if |video_frame| is null.
  void Paint(media::VideoFrame* video_frame,
             SkCanvas* canvas,
             const gfx::RectF& dest_rect,
             uint8 alpha);

 private:
  // An RGB bitmap and corresponding timestamp of the previously converted
  // video frame data.
  SkBitmap last_frame_;
  base::TimeDelta last_frame_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(SkCanvasVideoRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SKCANVAS_VIDEO_RENDERER_H_
