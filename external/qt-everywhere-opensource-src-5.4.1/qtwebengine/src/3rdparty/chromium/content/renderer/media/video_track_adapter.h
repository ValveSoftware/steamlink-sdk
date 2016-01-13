// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_TRACK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_TRACK_ADAPTER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "media/base/video_frame.h"

namespace content {

// VideoTrackAdapter is a helper class used by MediaStreamVideoSource used for
// adapting the video resolution from a source implementation to the resolution
// a track requires. Different tracks can have different resolution constraints.
// The constraints can be set as max width and height as well as max and min
// aspect ratio.
// Video frames are delivered to a track using a VideoCaptureDeliverFrameCB on
// the IO-thread.
// Adaptations is done by wrapping the original media::VideoFrame in a new
// media::VideoFrame with a new visible_rect and natural_size.
class VideoTrackAdapter
    : public base::RefCountedThreadSafe<VideoTrackAdapter> {
 public:
  explicit VideoTrackAdapter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Register |track| to receive video frames in |frame_callback| with
  // a resolution within the boundaries of the arguments.
  // Must be called on the main render thread. |frame_callback| is guaranteed to
  // be released on the main render thread.
  void AddTrack(const MediaStreamVideoTrack* track,
                VideoCaptureDeliverFrameCB frame_callback,
                int max_width, int max_height,
                double min_aspect_ratio,
                double max_aspect_ratio);
  void RemoveTrack(const MediaStreamVideoTrack* track);

  // Delivers |frame| to all tracks that have registered a callback.
  // Must be called on the IO-thread.
  void DeliverFrameOnIO(
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format,
      const base::TimeTicks& estimated_capture_time);

  const scoped_refptr<base::MessageLoopProxy>& io_message_loop() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return io_message_loop_;
  }

 private:
  virtual ~VideoTrackAdapter();
  friend class base::RefCountedThreadSafe<VideoTrackAdapter>;

  void AddTrackOnIO(
      const MediaStreamVideoTrack* track,
      VideoCaptureDeliverFrameCB frame_callback,
      int max_width, int max_height,
      double min_aspect_ratio,
      double max_aspect_ratio);
  void RemoveTrackOnIO(const MediaStreamVideoTrack* track);

  // |thread_checker_| is bound to the main render thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // |renderer_task_runner_| is used to ensure that
  // VideoCaptureDeliverFrameCB is released on the main render thread.
  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  // VideoFrameResolutionAdapter is an inner class that is created on the main
  // render thread but operates on the IO-thread. It does the resolution
  // adaptation and delivers frames to all registered tracks on the IO-thread.
  class VideoFrameResolutionAdapter;
  typedef std::vector<scoped_refptr<VideoFrameResolutionAdapter> >
      FrameAdapters;
  FrameAdapters adapters_;

  DISALLOW_COPY_AND_ASSIGN(VideoTrackAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_TRACK_ADAPTER_H_
