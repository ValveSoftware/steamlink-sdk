// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_track_adapter.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/content_switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_util.h"

namespace content {

namespace {

// Amount of frame intervals to wait before considering the source as muted, for
// the first frame and under normal conditions, respectively. First frame might
// take longer to arrive due to source startup.
const float kFirstFrameTimeoutInFrameIntervals = 100.0f;
const float kNormalFrameTimeoutInFrameIntervals = 25.0f;

// Min delta time between two frames allowed without being dropped if a max
// frame rate is specified.
const double kMinTimeInMsBetweenFrames = 5;
// If the delta between two frames is bigger than this, we will consider it to
// be invalid and reset the fps calculation.
const double kMaxTimeInMsBetweenFrames = 1000;

// Empty method used for keeping a reference to the original media::VideoFrame
// in VideoFrameResolutionAdapter::DeliverFrame if cropping is needed.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {
}

void ResetCallbackOnMainRenderThread(
    std::unique_ptr<VideoCaptureDeliverFrameCB> callback) {
  // |callback| will be deleted when this exits.
}

}  // anonymous namespace

// VideoFrameResolutionAdapter is created on and lives on the IO-thread. It does
// the resolution adaptation and delivers frames to all registered tracks on the
// IO-thread. All method calls must be on the IO-thread.
class VideoTrackAdapter::VideoFrameResolutionAdapter
    : public base::RefCountedThreadSafe<VideoFrameResolutionAdapter> {
 public:
  // Setting |max_frame_rate| to 0.0, means that no frame rate limitation
  // will be done.
  VideoFrameResolutionAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
      const gfx::Size& max_size,
      double min_aspect_ratio,
      double max_aspect_ratio,
      double max_frame_rate);

  // Add |callback| to receive video frames on the IO-thread.
  // |callback| will however be released on the main render thread.
  void AddCallback(const MediaStreamVideoTrack* track,
                   const VideoCaptureDeliverFrameCB& callback);

  // Removes |callback| associated with |track| from receiving video frames if
  // |track| has been added. It is ok to call RemoveCallback even if the |track|
  // has not been added. The |callback| is released on the main render thread.
  void RemoveCallback(const MediaStreamVideoTrack* track);

  void DeliverFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const base::TimeTicks& estimated_capture_time);

  // Returns true if all arguments match with the output of this adapter.
  bool ConstraintsMatch(const gfx::Size& max_size,
                        double min_aspect_ratio,
                        double max_aspect_ratio,
                        double max_frame_rate) const;

  bool IsEmpty() const;

 private:
  virtual ~VideoFrameResolutionAdapter();
  friend class base::RefCountedThreadSafe<VideoFrameResolutionAdapter>;

  void DoDeliverFrame(const scoped_refptr<media::VideoFrame>& frame,
                      const base::TimeTicks& estimated_capture_time);

  // Returns |true| if the input frame rate is higher that the requested max
  // frame rate and |frame| should be dropped.
  bool MaybeDropFrame(const scoped_refptr<media::VideoFrame>& frame,
                      float source_frame_rate);

  // Bound to the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // The task runner where we will release VideoCaptureDeliverFrameCB
  // registered in AddCallback.
  const scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  const gfx::Size max_frame_size_;
  const double min_aspect_ratio_;
  const double max_aspect_ratio_;

  double frame_rate_;
  base::TimeDelta last_time_stamp_;
  double max_frame_rate_;
  double keep_frame_counter_;

  typedef std::pair<const MediaStreamVideoTrack*, VideoCaptureDeliverFrameCB>
      VideoIdCallbackPair;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameResolutionAdapter);
};

VideoTrackAdapter::
VideoFrameResolutionAdapter::VideoFrameResolutionAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> render_message_loop,
    const gfx::Size& max_size,
    double min_aspect_ratio,
    double max_aspect_ratio,
    double max_frame_rate)
    : renderer_task_runner_(render_message_loop),
      max_frame_size_(max_size),
      min_aspect_ratio_(min_aspect_ratio),
      max_aspect_ratio_(max_aspect_ratio),
      frame_rate_(MediaStreamVideoSource::kDefaultFrameRate),
      last_time_stamp_(base::TimeDelta::Max()),
      max_frame_rate_(max_frame_rate),
      keep_frame_counter_(0.0) {
  DCHECK(renderer_task_runner_.get());
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_GE(max_aspect_ratio_, min_aspect_ratio_);
  CHECK_NE(0, max_aspect_ratio_);

  const std::string max_fps_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebRtcMaxCaptureFramerate);
  if (!max_fps_str.empty()) {
    double value;
    if (base::StringToDouble(max_fps_str, &value) && value >= 0.0) {
      DVLOG(1) << "Overriding max frame rate.  Was=" << max_frame_rate
              << ", Now=" << value;
      max_frame_rate_ = value;
    } else {
      DLOG(ERROR) << "Unable to set max fps to " << max_fps_str;
    }
  }

  DVLOG(3) << "VideoFrameResolutionAdapter("
          << "{ max_width =" << max_frame_size_.width() << "}, "
          << "{ max_height =" << max_frame_size_.height() << "}, "
          << "{ min_aspect_ratio =" << min_aspect_ratio << "}, "
          << "{ max_aspect_ratio_ =" << max_aspect_ratio_ << "}"
          << "{ max_frame_rate_ =" << max_frame_rate_ << "}) ";
}

VideoTrackAdapter::
VideoFrameResolutionAdapter::~VideoFrameResolutionAdapter() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(callbacks_.empty());
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
      std::unique_ptr<VideoCaptureDeliverFrameCB> callback(
          new VideoCaptureDeliverFrameCB(it->second));
      callbacks_.erase(it);
      renderer_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ResetCallbackOnMainRenderThread,
                                base::Passed(&callback)));

      return;
    }
  }
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  if (!frame) {
    DLOG(ERROR) << "Incoming frame is not valid.";
    return;
  }

  double frame_rate;
  if (!frame->metadata()->GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                    &frame_rate)) {
    frame_rate = MediaStreamVideoSource::kUnknownFrameRate;
  }

  if (MaybeDropFrame(frame, frame_rate))
    return;

  // TODO(perkj): Allow cropping / scaling of textures once
  // http://crbug/362521 is fixed.
  if (frame->HasTextures() ||
      frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFERS) {
    DoDeliverFrame(frame, estimated_capture_time);
    return;
  }
  scoped_refptr<media::VideoFrame> video_frame(frame);
  const double input_ratio =
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

    const double resulting_ratio =
        static_cast<double>(desired_width) / desired_height;
    // Make sure |min_aspect_ratio_| < |requested_ratio| < |max_aspect_ratio_|.
    const double requested_ratio = std::max(
        std::min(resulting_ratio, max_aspect_ratio_), min_aspect_ratio_);

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

    const gfx::Size desired_size(desired_width, desired_height);

    // Get the largest centered rectangle with the same aspect ratio of
    // |desired_size| that fits entirely inside of |frame->visible_rect()|.
    // This will be the rect we need to crop the original frame to.
    // From this rect, the original frame can be scaled down to |desired_size|.
    const gfx::Rect region_in_frame =
        media::ComputeLetterboxRegion(frame->visible_rect(), desired_size);

    video_frame = media::VideoFrame::WrapVideoFrame(
        frame, frame->format(), region_in_frame, desired_size);
    if (!video_frame)
      return;
    video_frame->AddDestructionObserver(
        base::Bind(&ReleaseOriginalFrame, frame));

    DVLOG(3) << "desired size  " << desired_size.ToString()
             << " output natural size "
             << video_frame->natural_size().ToString()
             << " output visible rect  "
             << video_frame->visible_rect().ToString();
  }
  DoDeliverFrame(video_frame, estimated_capture_time);
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::ConstraintsMatch(
    const gfx::Size& max_size,
    double min_aspect_ratio,
    double max_aspect_ratio,
    double max_frame_rate) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return max_frame_size_ == max_size &&
      min_aspect_ratio_ == min_aspect_ratio &&
      max_aspect_ratio_ == max_aspect_ratio &&
      max_frame_rate_ == max_frame_rate;
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::IsEmpty() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return callbacks_.empty();
}

void VideoTrackAdapter::VideoFrameResolutionAdapter::DoDeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  for (const auto& callback : callbacks_)
    callback.second.Run(frame, estimated_capture_time);
}

bool VideoTrackAdapter::VideoFrameResolutionAdapter::MaybeDropFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    float source_frame_rate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Do not drop frames if max frame rate hasn't been specified or the source
  // frame rate is known and is lower than max.
  if (max_frame_rate_ == 0.0f ||
      (source_frame_rate > 0 && source_frame_rate <= max_frame_rate_)) {
    return false;
  }

  const double delta_ms =
      (frame->timestamp() - last_time_stamp_).InMillisecondsF();

  // Check if the time since the last frame is completely off.
  if (delta_ms < 0 || delta_ms > kMaxTimeInMsBetweenFrames) {
    // Reset |last_time_stamp_| and fps calculation.
    last_time_stamp_ = frame->timestamp();
    frame_rate_ = MediaStreamVideoSource::kDefaultFrameRate;
    keep_frame_counter_ = 0.0;
    return false;
  }

  if (delta_ms < kMinTimeInMsBetweenFrames) {
    // We have seen video frames being delivered from camera devices back to
    // back. The simple AR filter for frame rate calculation is too short to
    // handle that. http://crbug/394315
    // TODO(perkj): Can we come up with a way to fix the times stamps and the
    // timing when frames are delivered so all frames can be used?
    // The time stamps are generated by Chrome and not the actual device.
    // Most likely the back to back problem is caused by software and not the
    // actual camera.
    DVLOG(3) << "Drop frame since delta time since previous frame is "
             << delta_ms << "ms.";
    return true;
  }
  last_time_stamp_ = frame->timestamp();
  // Calculate the frame rate using a simple AR filter.
  // Use a simple filter with 0.1 weight of the current sample.
  frame_rate_ = 100 / delta_ms + 0.9 * frame_rate_;

  // Prefer to not drop frames.
  if (max_frame_rate_ + 0.5f > frame_rate_)
    return false;  // Keep this frame.

  // The input frame rate is higher than requested.
  // Decide if we should keep this frame or drop it.
  keep_frame_counter_ += max_frame_rate_ / frame_rate_;
  if (keep_frame_counter_ >= 1) {
    keep_frame_counter_ -= 1;
    // Keep the frame.
    return false;
  }
  DVLOG(3) << "Drop frame. Input frame_rate_ " << frame_rate_ << ".";
  return true;
}

VideoTrackAdapter::VideoTrackAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner),
      renderer_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      monitoring_frame_rate_(false),
      muted_state_(false),
      frame_counter_(0),
      source_frame_rate_(0.0f) {
  DCHECK(io_task_runner);
}

VideoTrackAdapter::~VideoTrackAdapter() {
  DCHECK(adapters_.empty());
}

void VideoTrackAdapter::AddTrack(const MediaStreamVideoTrack* track,
                                 VideoCaptureDeliverFrameCB frame_callback,
                                 int max_width,
                                 int max_height,
                                 double min_aspect_ratio,
                                 double max_aspect_ratio,
                                 double max_frame_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoTrackAdapter::AddTrackOnIO, this, track, frame_callback,
                 gfx::Size(max_width, max_height), min_aspect_ratio,
                 max_aspect_ratio, max_frame_rate));
}

void VideoTrackAdapter::AddTrackOnIO(const MediaStreamVideoTrack* track,
                                     VideoCaptureDeliverFrameCB frame_callback,
                                     const gfx::Size& max_frame_size,
                                     double min_aspect_ratio,
                                     double max_aspect_ratio,
                                     double max_frame_rate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  scoped_refptr<VideoFrameResolutionAdapter> adapter;
  for (const auto& frame_adapter : adapters_) {
    if (frame_adapter->ConstraintsMatch(max_frame_size, min_aspect_ratio,
                                        max_aspect_ratio, max_frame_rate)) {
      adapter = frame_adapter.get();
      break;
    }
  }
  if (!adapter.get()) {
    adapter = new VideoFrameResolutionAdapter(renderer_task_runner_,
                                              max_frame_size,
                                              min_aspect_ratio,
                                              max_aspect_ratio,
                                              max_frame_rate);
    adapters_.push_back(adapter);
  }

  adapter->AddCallback(track, frame_callback);
}

void VideoTrackAdapter::RemoveTrack(const MediaStreamVideoTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoTrackAdapter::RemoveTrackOnIO, this, track));
}

void VideoTrackAdapter::StartFrameMonitoring(
    double source_frame_rate,
    const OnMutedCallback& on_muted_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VideoTrackAdapter::OnMutedCallback bound_on_muted_callback =
      media::BindToCurrentLoop(on_muted_callback);

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoTrackAdapter::StartFrameMonitoringOnIO, this,
                            bound_on_muted_callback, source_frame_rate));
}

void VideoTrackAdapter::StopFrameMonitoring() {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoTrackAdapter::StopFrameMonitoringOnIO, this));
}

void VideoTrackAdapter::StartFrameMonitoringOnIO(
    const OnMutedCallback& on_muted_callback,
    double source_frame_rate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(!monitoring_frame_rate_);

  monitoring_frame_rate_ = true;

  // If the source does not know the frame rate, set one by default.
  if (source_frame_rate == 0.0f)
    source_frame_rate = MediaStreamVideoSource::kDefaultFrameRate;
  source_frame_rate_ = source_frame_rate;
  DVLOG(1) << "Monitoring frame creation, first (large) delay: "
      << (kFirstFrameTimeoutInFrameIntervals / source_frame_rate_) << "s";
  io_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&VideoTrackAdapter::CheckFramesReceivedOnIO, this,
                            on_muted_callback, frame_counter_),
      base::TimeDelta::FromSecondsD(kFirstFrameTimeoutInFrameIntervals /
                                    source_frame_rate_));
}

void VideoTrackAdapter::StopFrameMonitoringOnIO() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  monitoring_frame_rate_ = false;
}

void VideoTrackAdapter::RemoveTrackOnIO(const MediaStreamVideoTrack* track) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
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
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("video", "VideoTrackAdapter::DeliverFrameOnIO");
  ++frame_counter_;
  for (const auto& adapter : adapters_)
    adapter->DeliverFrame(frame, estimated_capture_time);
}

void VideoTrackAdapter::CheckFramesReceivedOnIO(
    const OnMutedCallback& set_muted_state_callback,
    uint64_t old_frame_counter_snapshot) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!monitoring_frame_rate_)
    return;

  DVLOG_IF(1, old_frame_counter_snapshot == frame_counter_)
      << "No frames have passed, setting source as Muted.";

  bool muted_state = old_frame_counter_snapshot == frame_counter_;
  if (muted_state_ != muted_state) {
    set_muted_state_callback.Run(muted_state);
    muted_state_ = muted_state;
  }

  io_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&VideoTrackAdapter::CheckFramesReceivedOnIO, this,
                            set_muted_state_callback, frame_counter_),
      base::TimeDelta::FromSecondsD(kNormalFrameTimeoutInFrameIntervals /
                                    source_frame_rate_));
}

}  // namespace content
