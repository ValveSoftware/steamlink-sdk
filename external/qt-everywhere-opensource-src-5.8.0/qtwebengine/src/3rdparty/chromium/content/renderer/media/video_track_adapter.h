// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_TRACK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_TRACK_ADAPTER_H_

#include <stdint.h>

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
  typedef base::Callback<void(bool mute_state)> OnMutedCallback;

  explicit VideoTrackAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Register |track| to receive video frames in |frame_callback| with
  // a resolution within the boundaries of the arguments.
  // Must be called on the main render thread. |frame_callback| is guaranteed to
  // be released on the main render thread.
  // |source_frame_rate| is used to calculate a prudent interval to check for
  // passing frames and inform of the result via |on_muted_state_callback|.
  void AddTrack(const MediaStreamVideoTrack* track,
                VideoCaptureDeliverFrameCB frame_callback,
                int max_width, int max_height,
                double min_aspect_ratio,
                double max_aspect_ratio,
                double max_frame_rate);
  void RemoveTrack(const MediaStreamVideoTrack* track);

  // Delivers |frame| to all tracks that have registered a callback.
  // Must be called on the IO-thread.
  void DeliverFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        base::TimeTicks estimated_capture_time);

  base::SingleThreadTaskRunner* io_task_runner() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return io_task_runner_.get();
  }

  // Start monitor that frames are delivered to this object. I.E, that
  // |DeliverFrameOnIO| is called with a frame rate of |source_frame_rate|.
  // |on_muted_callback| is triggered on the main render thread.
  void StartFrameMonitoring(double source_frame_rate,
                            const OnMutedCallback& on_muted_callback);
  void StopFrameMonitoring();

 private:
  virtual ~VideoTrackAdapter();
  friend class base::RefCountedThreadSafe<VideoTrackAdapter>;

  void AddTrackOnIO(
      const MediaStreamVideoTrack* track,
      VideoCaptureDeliverFrameCB frame_callback,
      const gfx::Size& max_frame_size,
      double min_aspect_ratio,
      double max_aspect_ratio,
      double max_frame_rate);
  void RemoveTrackOnIO(const MediaStreamVideoTrack* track);

  void StartFrameMonitoringOnIO(
    const OnMutedCallback& on_muted_state_callback,
    double source_frame_rate);
  void StopFrameMonitoringOnIO();

  // Compare |frame_counter_snapshot| with the current |frame_counter_|, and
  // inform of the situation (muted, not muted) via |set_muted_state_callback|.
  void CheckFramesReceivedOnIO(const OnMutedCallback& set_muted_state_callback,
                               uint64_t old_frame_counter_snapshot);

  // |thread_checker_| is bound to the main render thread.
  base::ThreadChecker thread_checker_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |renderer_task_runner_| is used to ensure that
  // VideoCaptureDeliverFrameCB is released on the main render thread.
  const scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  // VideoFrameResolutionAdapter is an inner class that is created on the main
  // render thread but operates on the IO-thread. It does the resolution
  // adaptation and delivers frames to all registered tracks on the IO-thread.
  class VideoFrameResolutionAdapter;
  typedef std::vector<scoped_refptr<VideoFrameResolutionAdapter> >
      FrameAdapters;
  FrameAdapters adapters_;

  // Set to true if frame monitoring has been started. It is only accessed on
  // the IO-thread.
  bool monitoring_frame_rate_;

  // Keeps track of it frames have been received. It is only accessed on the
  // IO-thread.
  bool muted_state_;

  // Running frame counter, accessed on the IO-thread.
  uint64_t frame_counter_;

  // Frame rate configured on the video source, accessed on the IO-thread.
  float source_frame_rate_;

  DISALLOW_COPY_AND_ASSIGN(VideoTrackAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_TRACK_ADAPTER_H_
