// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_renderer_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/limits.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"

namespace media {

VideoRendererImpl::VideoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    ScopedVector<VideoDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    const PaintCB& paint_cb,
    bool drop_frames)
    : task_runner_(task_runner),
      video_frame_stream_(task_runner, decoders.Pass(), set_decryptor_ready_cb),
      low_delay_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      frame_available_(&lock_),
      state_(kUninitialized),
      thread_(),
      pending_read_(false),
      drop_frames_(drop_frames),
      playback_rate_(0),
      paint_cb_(paint_cb),
      last_timestamp_(kNoTimestamp()),
      frames_decoded_(0),
      frames_dropped_(0),
      weak_factory_(this) {
  DCHECK(!paint_cb_.is_null());
}

VideoRendererImpl::~VideoRendererImpl() {
  base::AutoLock auto_lock(lock_);
  CHECK(state_ == kStopped || state_ == kUninitialized) << state_;
  CHECK(thread_.is_null());
}

void VideoRendererImpl::Play(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPrerolled, state_);
  state_ = kPlaying;
  callback.Run();
}

void VideoRendererImpl::Flush(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);
  flush_cb_ = callback;
  state_ = kFlushing;

  // This is necessary if the |video_frame_stream_| has already seen an end of
  // stream and needs to drain it before flushing it.
  ready_frames_.clear();
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;
  video_frame_stream_.Reset(
      base::Bind(&VideoRendererImpl::OnVideoFrameStreamResetDone,
                 weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::Stop(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == kUninitialized || state_ == kStopped) {
    callback.Run();
    return;
  }

  // TODO(scherkus): Consider invalidating |weak_factory_| and replacing
  // task-running guards that check |state_| with DCHECK().

  state_ = kStopped;

  statistics_cb_.Reset();
  max_time_cb_.Reset();
  DoStopOrError_Locked();

  // Clean up our thread if present.
  base::PlatformThreadHandle thread_to_join = base::PlatformThreadHandle();
  if (!thread_.is_null()) {
    // Signal the thread since it's possible to get stopped with the video
    // thread waiting for a read to complete.
    frame_available_.Signal();
    std::swap(thread_, thread_to_join);
  }

  if (!thread_to_join.is_null()) {
    base::AutoUnlock auto_unlock(lock_);
    base::PlatformThread::Join(thread_to_join);
  }

  video_frame_stream_.Stop(callback);
}

void VideoRendererImpl::SetPlaybackRate(float playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererImpl::Preroll(base::TimeDelta time,
                                const PipelineStatusCB& cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(!cb.is_null());
  DCHECK(preroll_cb_.is_null());
  DCHECK(state_ == kFlushed || state_ == kPlaying) << "state_ " << state_;

  if (state_ == kFlushed) {
    DCHECK(time != kNoTimestamp());
    DCHECK(!pending_read_);
    DCHECK(ready_frames_.empty());
  } else {
    DCHECK(time == kNoTimestamp());
  }

  state_ = kPrerolling;
  preroll_cb_ = cb;
  preroll_timestamp_ = time;

  if (ShouldTransitionToPrerolled_Locked()) {
    TransitionToPrerolled_Locked();
    return;
  }

  AttemptRead_Locked();
}

void VideoRendererImpl::Initialize(DemuxerStream* stream,
                                   bool low_delay,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const TimeCB& max_time_cb,
                                   const base::Closure& ended_cb,
                                   const PipelineStatusCB& error_cb,
                                   const TimeDeltaCB& get_time_cb,
                                   const TimeDeltaCB& get_duration_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!max_time_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!get_time_cb.is_null());
  DCHECK(!get_duration_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);

  low_delay_ = low_delay;

  init_cb_ = init_cb;
  statistics_cb_ = statistics_cb;
  max_time_cb_ = max_time_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  get_time_cb_ = get_time_cb;
  get_duration_cb_ = get_duration_cb;
  state_ = kInitializing;

  video_frame_stream_.Initialize(
      stream,
      low_delay,
      statistics_cb,
      base::Bind(&VideoRendererImpl::OnVideoFrameStreamInitialized,
                 weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::OnVideoFrameStreamInitialized(bool success) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kInitializing);

  if (!success) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // We're all good!  Consider ourselves flushed. (ThreadMain() should never
  // see us in the kUninitialized state).
  // Since we had an initial Preroll(), we consider ourself flushed, because we
  // have not populated any buffers yet.
  state_ = kFlushed;

  // Create our video thread.
  CHECK(base::PlatformThread::Create(0, this, &thread_));

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_.platform_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

// PlatformThread::Delegate implementation.
void VideoRendererImpl::ThreadMain() {
  base::PlatformThread::SetName("CrVideoRenderer");

  // The number of milliseconds to idle when we do not have anything to do.
  // Nothing special about the value, other than we're being more OS-friendly
  // than sleeping for 1 millisecond.
  //
  // TODO(scherkus): switch to pure event-driven frame timing instead of this
  // kIdleTimeDelta business http://crbug.com/106874
  const base::TimeDelta kIdleTimeDelta =
      base::TimeDelta::FromMilliseconds(10);

  for (;;) {
    base::AutoLock auto_lock(lock_);

    // Thread exit condition.
    if (state_ == kStopped)
      return;

    // Remain idle as long as we're not playing.
    if (state_ != kPlaying || playback_rate_ == 0) {
      UpdateStatsAndWait_Locked(kIdleTimeDelta);
      continue;
    }

    // Remain idle until we have the next frame ready for rendering.
    if (ready_frames_.empty()) {
      if (received_end_of_stream_ && !rendered_end_of_stream_) {
        rendered_end_of_stream_ = true;
        ended_cb_.Run();
      }

      UpdateStatsAndWait_Locked(kIdleTimeDelta);
      continue;
    }

    base::TimeDelta remaining_time =
        CalculateSleepDuration(ready_frames_.front(), playback_rate_);

    // Sleep up to a maximum of our idle time until we're within the time to
    // render the next frame.
    if (remaining_time.InMicroseconds() > 0) {
      remaining_time = std::min(remaining_time, kIdleTimeDelta);
      UpdateStatsAndWait_Locked(remaining_time);
      continue;
    }

    // Deadline is defined as the midpoint between this frame and the next
    // frame, using the delta between this frame and the previous frame as the
    // assumption for frame duration.
    //
    // TODO(scherkus): An improvement over midpoint might be selecting the
    // minimum and/or maximum between the midpoint and some constants. As a
    // thought experiment, consider what would be better than the midpoint
    // for both the 1fps case and 120fps case.
    //
    // TODO(scherkus): This can be vastly improved. Use a histogram to measure
    // the accuracy of our frame timing code. http://crbug.com/149829
    if (drop_frames_ && last_timestamp_ != kNoTimestamp()) {
      base::TimeDelta now = get_time_cb_.Run();
      base::TimeDelta deadline = ready_frames_.front()->timestamp() +
          (ready_frames_.front()->timestamp() - last_timestamp_) / 2;

      if (now > deadline) {
        DropNextReadyFrame_Locked();
        continue;
      }
    }

    // Congratulations! You've made it past the video frame timing gauntlet.
    //
    // At this point enough time has passed that the next frame that ready for
    // rendering.
    PaintNextReadyFrame_Locked();
  }
}

void VideoRendererImpl::PaintNextReadyFrame_Locked() {
  lock_.AssertAcquired();

  scoped_refptr<VideoFrame> next_frame = ready_frames_.front();
  ready_frames_.pop_front();
  frames_decoded_++;

  last_timestamp_ = next_frame->timestamp();

  paint_cb_.Run(next_frame);

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoRendererImpl::AttemptRead, weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::DropNextReadyFrame_Locked() {
  TRACE_EVENT0("media", "VideoRendererImpl:frameDropped");

  lock_.AssertAcquired();

  last_timestamp_ = ready_frames_.front()->timestamp();
  ready_frames_.pop_front();
  frames_decoded_++;
  frames_dropped_++;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoRendererImpl::AttemptRead, weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::FrameReady(VideoFrameStream::Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kFlushed);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == VideoFrameStream::DECODE_ERROR ||
      status == VideoFrameStream::DECRYPT_ERROR) {
    DCHECK(!frame.get());
    PipelineStatus error = PIPELINE_ERROR_DECODE;
    if (status == VideoFrameStream::DECRYPT_ERROR)
      error = PIPELINE_ERROR_DECRYPT;

    if (!preroll_cb_.is_null()) {
      base::ResetAndReturn(&preroll_cb_).Run(error);
      return;
    }

    error_cb_.Run(error);
    return;
  }

  // Already-queued VideoFrameStream ReadCB's can fire after various state
  // transitions have happened; in that case just drop those frames immediately.
  if (state_ == kStopped || state_ == kFlushing)
    return;

  if (!frame.get()) {
    // Abort preroll early for a NULL frame because we won't get more frames.
    // A new preroll will be requested after this one completes so there is no
    // point trying to collect more frames.
    if (state_ == kPrerolling)
      TransitionToPrerolled_Locked();

    return;
  }

  if (frame->end_of_stream()) {
    DCHECK(!received_end_of_stream_);
    received_end_of_stream_ = true;
    max_time_cb_.Run(get_duration_cb_.Run());

    if (state_ == kPrerolling)
      TransitionToPrerolled_Locked();

    return;
  }

  // Maintain the latest frame decoded so the correct frame is displayed after
  // prerolling has completed.
  if (state_ == kPrerolling && preroll_timestamp_ != kNoTimestamp() &&
      frame->timestamp() <= preroll_timestamp_) {
    ready_frames_.clear();
  }

  AddReadyFrame_Locked(frame);

  if (ShouldTransitionToPrerolled_Locked())
    TransitionToPrerolled_Locked();

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
  AttemptRead_Locked();
}

bool VideoRendererImpl::ShouldTransitionToPrerolled_Locked() {
  return state_ == kPrerolling &&
      (!video_frame_stream_.CanReadWithoutStalling() ||
       ready_frames_.size() >= static_cast<size_t>(limits::kMaxVideoFrames) ||
       (low_delay_ && ready_frames_.size() > 0));
}

void VideoRendererImpl::AddReadyFrame_Locked(
    const scoped_refptr<VideoFrame>& frame) {
  lock_.AssertAcquired();
  DCHECK(!frame->end_of_stream());

  // Adjust the incoming frame if its rendering stop time is past the duration
  // of the video itself. This is typically the last frame of the video and
  // occurs if the container specifies a duration that isn't a multiple of the
  // frame rate.  Another way for this to happen is for the container to state
  // a smaller duration than the largest packet timestamp.
  base::TimeDelta duration = get_duration_cb_.Run();
  if (frame->timestamp() > duration) {
    frame->set_timestamp(duration);
  }

  ready_frames_.push_back(frame);
  DCHECK_LE(ready_frames_.size(),
            static_cast<size_t>(limits::kMaxVideoFrames));

  max_time_cb_.Run(frame->timestamp());

  // Avoid needlessly waking up |thread_| unless playing.
  if (state_ == kPlaying)
    frame_available_.Signal();
}

void VideoRendererImpl::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void VideoRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (pending_read_ || received_end_of_stream_ ||
      ready_frames_.size() == static_cast<size_t>(limits::kMaxVideoFrames)) {
    return;
  }

  switch (state_) {
    case kPrerolling:
    case kPrerolled:
    case kPlaying:
      pending_read_ = true;
      video_frame_stream_.Read(base::Bind(&VideoRendererImpl::FrameReady,
                                          weak_factory_.GetWeakPtr()));
      return;

    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
    case kStopped:
      return;
  }
}

void VideoRendererImpl::OnVideoFrameStreamResetDone() {
  base::AutoLock auto_lock(lock_);
  if (state_ == kStopped)
    return;

  DCHECK_EQ(kFlushing, state_);
  DCHECK(!pending_read_);
  DCHECK(ready_frames_.empty());
  DCHECK(!received_end_of_stream_);
  DCHECK(!rendered_end_of_stream_);

  state_ = kFlushed;
  last_timestamp_ = kNoTimestamp();
  base::ResetAndReturn(&flush_cb_).Run();
}

base::TimeDelta VideoRendererImpl::CalculateSleepDuration(
    const scoped_refptr<VideoFrame>& next_frame,
    float playback_rate) {
  // Determine the current and next presentation timestamps.
  base::TimeDelta now = get_time_cb_.Run();
  base::TimeDelta next_pts = next_frame->timestamp();

  // Scale our sleep based on the playback rate.
  base::TimeDelta sleep = next_pts - now;
  return base::TimeDelta::FromMicroseconds(
      static_cast<int64>(sleep.InMicroseconds() / playback_rate));
}

void VideoRendererImpl::DoStopOrError_Locked() {
  lock_.AssertAcquired();
  last_timestamp_ = kNoTimestamp();
  ready_frames_.clear();
}

void VideoRendererImpl::TransitionToPrerolled_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(state_, kPrerolling);

  state_ = kPrerolled;

  // Because we might remain in the prerolled state for an undetermined amount
  // of time (e.g., we seeked while paused), we'll paint the first prerolled
  // frame.
  if (!ready_frames_.empty())
    PaintNextReadyFrame_Locked();

  base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
}

void VideoRendererImpl::UpdateStatsAndWait_Locked(
    base::TimeDelta wait_duration) {
  lock_.AssertAcquired();
  DCHECK_GE(frames_decoded_, 0);
  DCHECK_LE(frames_dropped_, frames_decoded_);

  if (frames_decoded_) {
    PipelineStatistics statistics;
    statistics.video_frames_decoded = frames_decoded_;
    statistics.video_frames_dropped = frames_dropped_;
    statistics_cb_.Run(statistics);

    frames_decoded_ = 0;
    frames_dropped_ = 0;
  }

  frame_available_.TimedWait(wait_duration);
}

}  // namespace media
