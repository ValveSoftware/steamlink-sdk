// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_video_webrtc_sink.h"

#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/renderer/media_stream_utils.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "media/base/limits.h"

namespace content {

namespace {

// The default number of microseconds that should elapse since the last video
// frame was received, before requesting a refresh frame.
const int64_t kDefaultRefreshIntervalMicros =
    base::Time::kMicrosecondsPerSecond;

// A lower-bound for the refresh interval.
const int64_t kLowerBoundRefreshIntervalMicros =
    base::Time::kMicrosecondsPerSecond / media::limits::kMaxFramesPerSecond;

}  // namespace

// Simple help class used for receiving video frames on the IO-thread from a
// MediaStreamVideoTrack and forward the frames to a WebRtcVideoCapturerAdapter
// on libjingle's worker thread. WebRtcVideoCapturerAdapter implements a video
// capturer for libjingle.
class MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter
    : public base::RefCountedThreadSafe<WebRtcVideoSourceAdapter> {
 public:
  WebRtcVideoSourceAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          libjingle_worker_thread,
      const scoped_refptr<webrtc::VideoTrackSourceInterface>& source,
      WebRtcVideoCapturerAdapter* capture_adapter,
      base::TimeDelta refresh_interval,
      const base::Closure& refresh_callback);

  // MediaStreamVideoWebRtcSink can be destroyed on the main render thread or
  // libjingles worker thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is to
  // make sure |video_source_| is released when MediaStreamVideoWebRtcSink() is
  // destroyed.
  void ReleaseSourceOnMainThread();

  void OnVideoFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        base::TimeTicks estimated_capture_time);

 private:
  void OnVideoFrameOnWorkerThread(
      const scoped_refptr<media::VideoFrame>& frame);
  friend class base::RefCountedThreadSafe<WebRtcVideoSourceAdapter>;
  virtual ~WebRtcVideoSourceAdapter();

  // Called whenever a video frame was just delivered on the IO thread. This
  // restarts the delay period before the |refresh_timer_| will fire the next
  // time.
  void ResetRefreshTimerOnMainThread();

  scoped_refptr<base::SingleThreadTaskRunner> render_thread_task_runner_;

  // |render_thread_checker_| is bound to the main render thread.
  base::ThreadChecker render_thread_checker_;
  // Used to DCHECK that frames are called on the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // Used for posting frames to libjingle's worker thread. Accessed on the
  // IO-thread.
  scoped_refptr<base::SingleThreadTaskRunner> libjingle_worker_thread_;

  scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_;

  // Used to protect |capture_adapter_|. It is taken by libjingle's worker
  // thread for each video frame that is delivered but only taken on the
  // main render thread in ReleaseSourceOnMainThread() when
  // the owning MediaStreamVideoWebRtcSink is being destroyed.
  base::Lock capture_adapter_stop_lock_;
  // |capture_adapter_| is owned by |video_source_|
  WebRtcVideoCapturerAdapter* capture_adapter_;

  // Requests a refresh frame at regular intervals. The delay on this timer is
  // reset each time a frame is received so that it will not fire for at least
  // an additional period. This means refresh frames will only be requested when
  // the source has halted delivery (e.g., a screen capturer stops sending
  // frames because the screen is not being updated).
  //
  // This mechanism solves a number of problems. First, it will ensure that
  // remote clients that join a distributed session receive a first video frame
  // in a timely manner. Second, it will allow WebRTC's internal bandwidth
  // estimation logic to maintain a more optimal state, since sending a video
  // frame will "prime it." Third, it allows lossy encoders to clean up
  // artifacts in a still image.  http://crbug.com/486274
  base::RepeatingTimer refresh_timer_;
};

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& libjingle_worker_thread,
    const scoped_refptr<webrtc::VideoTrackSourceInterface>& source,
    WebRtcVideoCapturerAdapter* capture_adapter,
    base::TimeDelta refresh_interval,
    const base::Closure& refresh_callback)
    : render_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      libjingle_worker_thread_(libjingle_worker_thread),
      video_source_(source),
      capture_adapter_(capture_adapter) {
  io_thread_checker_.DetachFromThread();
  if (!refresh_interval.is_zero()) {
    VLOG(1) << "Starting frame refresh timer with interval "
            << refresh_interval.InMillisecondsF() << " ms.";
    refresh_timer_.Start(FROM_HERE, refresh_interval, refresh_callback);
  }
}

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ~WebRtcVideoSourceAdapter() {
  DVLOG(3) << "~WebRtcVideoSourceAdapter()";
  DCHECK(!capture_adapter_);
  // This object can be destroyed on the main render thread or libjingles worker
  // thread since it posts video frames on that thread. But |video_source_| must
  // be released on the main render thread before the PeerConnectionFactory has
  // been destroyed. The only way to ensure that is to make sure |video_source_|
  // is released when MediaStreamVideoWebRtcSink() is destroyed.
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
ResetRefreshTimerOnMainThread() {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  if (refresh_timer_.IsRunning())
    refresh_timer_.Reset();
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
ReleaseSourceOnMainThread() {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  // Since frames are posted to the worker thread, this object might be deleted
  // on that thread. However, since |video_source_| was created on the render
  // thread, it should be released on the render thread.
  base::AutoLock auto_lock(capture_adapter_stop_lock_);
  // |video_source| owns |capture_adapter_|.
  capture_adapter_ = NULL;
  video_source_ = NULL;
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::OnVideoFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  render_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcVideoSourceAdapter::ResetRefreshTimerOnMainThread,
                 this));
  libjingle_worker_thread_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread,
                 this,
                 frame));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnVideoFrameOnWorkerThread(const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(libjingle_worker_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(capture_adapter_stop_lock_);
  if (capture_adapter_)
    capture_adapter_->OnFrameCaptured(frame);
}

MediaStreamVideoWebRtcSink::MediaStreamVideoWebRtcSink(
    const blink::WebMediaStreamTrack& track,
    PeerConnectionDependencyFactory* factory)
    : weak_factory_(this) {
  const blink::WebMediaConstraints& constraints =
      MediaStreamVideoTrack::GetVideoTrack(track)->constraints();

  // Check for presence of mediaStreamSource constraint. The value is ignored.
  std::string value;
  bool is_screencast = GetConstraintValueAsString(
      constraints, &blink::WebMediaTrackConstraintSet::mediaStreamSource,
      &value);

  // Enable automatic frame refreshes for the screen capture sources, which will
  // stop producing frames whenever screen content is not changing. Check the
  // frameRate constraint to determine the rate of refreshes. If a minimum
  // frameRate is provided, use that. Otherwise, use the maximum frameRate if it
  // happens to be less than the default.
  base::TimeDelta refresh_interval = base::TimeDelta::FromMicroseconds(0);
  if (is_screencast) {
    // Start with the default refresh interval, and refine based on constraints.
    refresh_interval =
        base::TimeDelta::FromMicroseconds(kDefaultRefreshIntervalMicros);
    double value = 0.0;
    if (GetConstraintMinAsDouble(
            constraints, &blink::WebMediaTrackConstraintSet::frameRate,
            &value) &&
        value > 0.0) {
      refresh_interval =
          base::TimeDelta::FromMicroseconds(base::saturated_cast<int64_t>(
              base::Time::kMicrosecondsPerSecond / value));
    }
    if (GetConstraintMaxAsDouble(
            constraints, &blink::WebMediaTrackConstraintSet::frameRate,
            &value) &&
        value > 0.0) {
      const base::TimeDelta alternate_refresh_interval =
          base::TimeDelta::FromMicroseconds(base::saturated_cast<int64_t>(
              base::Time::kMicrosecondsPerSecond / value));
      refresh_interval = std::max(refresh_interval, alternate_refresh_interval);
    }
    if (refresh_interval.InMicroseconds() < kLowerBoundRefreshIntervalMicros) {
      refresh_interval =
          base::TimeDelta::FromMicroseconds(kLowerBoundRefreshIntervalMicros);
    }
  }

  WebRtcVideoCapturerAdapter* capture_adapter =
      factory->CreateVideoCapturer(is_screencast);

  // |video_source| owns |capture_adapter|
  scoped_refptr<webrtc::VideoTrackSourceInterface> video_source(
      factory->CreateVideoSource(capture_adapter));

  video_track_ = factory->CreateLocalVideoTrack(track.id().utf8(),
                                                video_source.get());

  video_track_->set_enabled(track.isEnabled());

  source_adapter_ = new WebRtcVideoSourceAdapter(
      factory->GetWebRtcWorkerThread(),
      video_source,
      capture_adapter,
      refresh_interval,
      base::Bind(&MediaStreamVideoWebRtcSink::RequestRefreshFrame,
                 weak_factory_.GetWeakPtr()));

  MediaStreamVideoSink::ConnectToTrack(
      track,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnIO, source_adapter_),
      false);

  DVLOG(3) << "MediaStreamVideoWebRtcSink ctor() : is_screencast "
           << is_screencast;
}

MediaStreamVideoWebRtcSink::~MediaStreamVideoWebRtcSink() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "MediaStreamVideoWebRtcSink dtor().";
  weak_factory_.InvalidateWeakPtrs();
  MediaStreamVideoSink::DisconnectFromTrack();
  source_adapter_->ReleaseSourceOnMainThread();
}

void MediaStreamVideoWebRtcSink::OnEnabledChanged(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_track_->set_enabled(enabled);
}

void MediaStreamVideoWebRtcSink::RequestRefreshFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  content::RequestRefreshFrameFromVideoTrack(connected_track());
}

}  // namespace content
