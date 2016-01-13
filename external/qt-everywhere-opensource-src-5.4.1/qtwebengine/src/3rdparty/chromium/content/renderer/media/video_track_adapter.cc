// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_track_adapter.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "media/base/video_util.h"

namespace content {

namespace {

// Empty method used for keeping a reference to the original media::VideoFrame
// in VideoFrameResolutionAdapter::DeliverFrame if cropping is needed.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void ResetCallbackOnMainRenderThread(
    scoped_ptr<VideoCaptureDeliverFrameCB> callback) {
  // |callback| will be deleted when this exits.
}

}  // anonymous namespace

// VideoFrameResolutionAdapter is created on and lives on
// on the IO-thread. It does the resolution adaptation and delivers frames to
// all registered tracks on the IO-thread.
// All method calls must be on the IO-thread.
class VideoTrackAdapter::VideoFrameResolutionAdapter
    : public base::RefCountedThreadSafe<VideoFrameResolutionAdapter> {
 public:
  VideoFrameResolutionAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
      int max_width,
      int max_height,
      double min_aspect_ratio,
      double max_aspect_ratio);

  // Add |callback| to receive video frames on the IO-thread.
  // |callback| will however be released on the main render thread.
  void AddCallback(const MediaStreamVideoTrack* track,
                   const VideoCaptureDeliverFrameCB& callback);

  // Removes |callback| associated with |track| from receiving video frames if
  // |track| has been added. It is ok to call RemoveCallback even if the |track|
  // has not been added. The |callback| is released on the main render thread.
  void RemoveCallback(const MediaStreamVideoTrack* track);

  void DeliverFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const media::VideoCaptureFormat& format,
                    const base::TimeTicks& estimated_capture_time);

  // Returns true if all arguments match with the output of this adapter.
  bool ConstraintsMatch(int max_width,
                        int max_height,
                        double min_aspect_ratio,
                        double max_aspect_ratio) const;

  bool IsEmpty() const;

 private:
  virtual ~VideoFrameResolutionAdapter();
  friend class base::RefCountedThreadSafe<VideoFrameResolutionAdapter>;

  virtual void DoDeliverFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format,
      const base::TimeTicks& estimated_capture_time);

  // Bound to the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // The task runner where we will release VideoCaptureDeliverFrameCB
  // registered in AddCallback.
  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  gfx::Size max_frame_size_;
  double min_aspect_ratio_;
  double max_aspect_ratio_;

  typedef std::pair<const void*, VideoCaptureDeliverFrameCB>
      VideoIdCallbackPair;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResolutionAdapter);
};

VideoTrackAdapter::
VideoFrameResolutionAdapter::VideoFrameResolutionAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
    int max_width,
    int max_height,
    double min_aspect_ratio,
    double max_aspect_ratio)
    : renderer_task_runner_(render_message_loop),
      max_frame_size_(max_width, max_height),
      min_aspect_ratio_(min_aspect_ratio),
      max_aspect_ratio_(max_aspect_ratio) {
  DCHECK(renderer_task_runner_);
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_GE(max_aspect_ratio_, min_aspect_ratio_);
  CHECK_NE(0, max_aspect_ratio_);
  DVLOG(3) << "VideoFrameResolutionAdapter("
          << "{ max_width =" << max_width << "}, "
          << "{ max_height =" << max_height << "}, "
          << "{ min_aspect_ratio =" << min_aspect_ratio << "}, "
          << "{ max_aspect_ratio_ =" << max_aspect_ratio_ << "}) ";
}

VideoTrackAdapter::
VideoFrameResolutionAdapter::~VideoFrameResolutionAdapter() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(callbacks_.empty());
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(perkj): Allow cropping / scaling of textures once
  // http://crbug/362521 is fixed.
  if (frame->format() == media::VideoFrame::NATIVE_TEXTURE) {
    DoDeliverFrame(frame, format, estimated_capture_time);
    return;
  }
  scoped_refptr<media::VideoFrame> video_frame(frame);
  double input_ratio =
      static_cast<double>(frame->natural_size().width()) /
      frame->natural_size().height();

  // If |frame| has larger width or height than requested, or the aspect ratio
  // does not match the requested, we want to create a wrapped version of this
  // frame with a size that fulfills the constraints.
  if (frame->natural_size().width() > max_frame_size_.width() ||
      frame->natural_size().height() > max_frame_size_.height() ||
      input_ratio > max_aspect_ratio_ ||
      input_ratio < min_aspect_ratio_) {
    int desired_width = std::min(max_frame_size_.width(),
                                 frame->natural_size().width());
    int desired_height = std::min(max_frame_size_.height(),
                                  frame->natural_size().height());

    double resulting_ratio =
        static_cast<double>(desired_width) / desired_height;
    double requested_ratio = resulting_ratio;

    if (requested_ratio > max_aspect_ratio_)
      requested_ratio = max_aspect_ratio_;
    else if (requested_ratio < min_aspect_ratio_)
      requested_ratio = min_aspect_ratio_;

    if (resulting_ratio < requested_ratio) {
      desired_height = static_cast<int>((desired_height * resulting_ratio) /
                                        requested_ratio);
      // Make sure we scale to an even height to avoid rounding errors
      desired_height = (desired_height + 1) & ~1;
    } else if (resulting_ratio > requested_ratio) {
      desired_width = static_cast<int>((desired_width * requested_ratio) /
                                       resulting_ratio);
      // Make sure we scale to an even width to avoid rounding errors.
      desired_width = (desired_width + 1) & ~1;
    }

    gfx::Size desired_size(desired_width, desired_height);

    // Get the largest centered rectangle with the same aspect ratio of
    // |desired_size| that fits entirely inside of |frame->visible_rect()|.
    // This will be the rect we need to crop the original frame to.
    // From this rect, the original frame can be scaled down to |desired_size|.
    gfx::Rect region_in_frame =
        media::ComputeLetterboxRegion(frame->visible_rect(), desired_size);

    video_frame = media::VideoFrame::WrapVideoFrame(
        frame,
        region_in_frame,
        desired_size,
        base::Bind(&ReleaseOriginalFrame, frame));

    DVLOG(3) << "desired size  " << desired_size.ToString()
             << " output natural size "
             << video_frame->natural_size().ToString()
             << " output visible rect  "
             << video_frame->visible_rect().ToString();
  }
  DoDeliverFrame(video_frame, format, estimated_capture_time);
}

void VideoTrackAdapter::
VideoFrameResolutionAdapter::DoDeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  for (std::vector<VideoIdCallbackPair>::const_iterator it = callbacks_.begin();
       it != callbacks_.end(); ++it) {
    it->second.Run(frame, format, estimated_capture_time);
  }
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::AddCallback(
    const MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  callbacks_.push_back(std::make_pair(track, callback));
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::RemoveCallback(
    const MediaStreamVideoTrack* track) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  std::vector<VideoIdCallbackPair>::iterator it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == track) {
      // Make sure the VideoCaptureDeliverFrameCB is released on the main
      // render thread since it was added on the main render thread in
      // VideoTrackAdapter::AddTrack.
      scoped_ptr<VideoCaptureDeliverFrameCB> callback(
          new VideoCaptureDeliverFrameCB(it->second));
      callbacks_.erase(it);
      renderer_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ResetCallbackOnMainRenderThread,
                                base::Passed(&callback)));

      return;
    }
  }
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::ConstraintsMatch(
    int max_width,
    int max_height,
    double min_aspect_ratio,
    double max_aspect_ratio) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return max_frame_size_.width() == max_width &&
      max_frame_size_.height() == max_height &&
      min_aspect_ratio_ == min_aspect_ratio &&
      max_aspect_ratio_ == max_aspect_ratio;
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::IsEmpty() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return callbacks_.empty();
}

VideoTrackAdapter::VideoTrackAdapter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : io_message_loop_(io_message_loop),
      renderer_task_runner_(base::MessageLoopProxy::current()) {
  DCHECK(io_message_loop_);
}

VideoTrackAdapter::~VideoTrackAdapter() {
  DCHECK(adapters_.empty());
}

void VideoTrackAdapter::AddTrack(const MediaStreamVideoTrack* track,
                                 VideoCaptureDeliverFrameCB frame_callback,
                                 int max_width,
                                 int max_height,
                                 double min_aspect_ratio,
                                 double max_aspect_ratio) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoTrackAdapter::AddTrackOnIO,
                 this, track, frame_callback, max_width, max_height,
                 min_aspect_ratio, max_aspect_ratio));
}

void VideoTrackAdapter::AddTrackOnIO(
    const MediaStreamVideoTrack* track,
    VideoCaptureDeliverFrameCB frame_callback,
    int max_width,
    int max_height,
    double min_aspect_ratio,
    double max_aspect_ratio) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  scoped_refptr<VideoFrameResolutionAdapter> adapter;
  for (FrameAdapters::const_iterator it = adapters_.begin();
       it != adapters_.end(); ++it) {
    if ((*it)->ConstraintsMatch(max_width, max_height, min_aspect_ratio,
                                max_aspect_ratio)) {
      adapter = it->get();
      break;
    }
  }
  if (!adapter) {
    adapter = new VideoFrameResolutionAdapter(renderer_task_runner_,
                                              max_width,
                                              max_height,
                                              min_aspect_ratio,
                                              max_aspect_ratio);
    adapters_.push_back(adapter);
  }

  adapter->AddCallback(track, frame_callback);
}

void VideoTrackAdapter::RemoveTrack(const MediaStreamVideoTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoTrackAdapter::RemoveTrackOnIO, this, track));
}

void VideoTrackAdapter::RemoveTrackOnIO(const MediaStreamVideoTrack* track) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  for (FrameAdapters::iterator it = adapters_.begin();
       it != adapters_.end(); ++it) {
    (*it)->RemoveCallback(track);
    if ((*it)->IsEmpty()) {
      adapters_.erase(it);
      break;
    }
  }
}

void VideoTrackAdapter::DeliverFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  TRACE_EVENT0("video", "VideoTrackAdapter::DeliverFrameOnIO");
  for (FrameAdapters::iterator it = adapters_.begin();
       it != adapters_.end(); ++it) {
    (*it)->DeliverFrame(frame, format, estimated_capture_time);
  }
}

}  // namespace content
