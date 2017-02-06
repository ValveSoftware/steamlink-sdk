// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_renderer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/video_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"

namespace media {

VideoRendererImpl::VideoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    VideoRendererSink* sink,
    ScopedVector<VideoDecoder> decoders,
    bool drop_frames,
    GpuVideoAcceleratorFactories* gpu_factories,
    const scoped_refptr<MediaLog>& media_log)
    : task_runner_(media_task_runner),
      sink_(sink),
      sink_started_(false),
      client_(nullptr),
      video_frame_stream_(new VideoFrameStream(media_task_runner,
                                               std::move(decoders),
                                               media_log)),
      gpu_memory_buffer_pool_(nullptr),
      media_log_(media_log),
      low_delay_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      state_(kUninitialized),
      pending_read_(false),
      drop_frames_(drop_frames),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      frames_decoded_(0),
      frames_dropped_(0),
      tick_clock_(new base::DefaultTickClock()),
      was_background_rendering_(false),
      time_progressing_(false),
      last_video_memory_usage_(0),
      have_renderered_frames_(false),
      last_frame_opaque_(false),
      weak_factory_(this),
      frame_callback_weak_factory_(this) {
  if (gpu_factories &&
      gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames()) {
    gpu_memory_buffer_pool_.reset(new GpuMemoryBufferVideoFramePool(
        media_task_runner, worker_task_runner, gpu_factories));
  }
}

VideoRendererImpl::~VideoRendererImpl() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();

  if (sink_started_)
    StopSink();
}

void VideoRendererImpl::Flush(const base::Closure& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (sink_started_)
    StopSink();

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  flush_cb_ = callback;
  state_ = kFlushing;

  // This is necessary if the |video_frame_stream_| has already seen an end of
  // stream and needs to drain it before flushing it.
  if (buffering_state_ != BUFFERING_HAVE_NOTHING) {
    buffering_state_ = BUFFERING_HAVE_NOTHING;
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&VideoRendererImpl::OnBufferingStateChange,
                              weak_factory_.GetWeakPtr(), buffering_state_));
  }
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  algorithm_->Reset();

  video_frame_stream_->Reset(
      base::Bind(&VideoRendererImpl::OnVideoFrameStreamResetDone,
                 weak_factory_.GetWeakPtr()));
}

void VideoRendererImpl::StartPlayingFrom(base::TimeDelta timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InMicroseconds() << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK(!pending_read_);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  state_ = kPlaying;
  start_timestamp_ = timestamp;
  AttemptRead_Locked();
}

void VideoRendererImpl::Initialize(
    DemuxerStream* stream,
    CdmContext* cdm_context,
    RendererClient* client,
    const TimeSource::WallClockTimeCB& wall_clock_time_cb,
    const PipelineStatusCB& init_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(!init_cb.is_null());
  DCHECK(!wall_clock_time_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(!was_background_rendering_);
  DCHECK(!time_progressing_);
  DCHECK(!have_renderered_frames_);

  low_delay_ = (stream->liveness() == DemuxerStream::LIVENESS_LIVE);
  UMA_HISTOGRAM_BOOLEAN("Media.VideoRenderer.LowDelay", low_delay_);
  if (low_delay_)
    MEDIA_LOG(DEBUG, media_log_) << "Video rendering in low delay mode.";

  // Always post |init_cb_| because |this| could be destroyed if initialization
  // failed.
  init_cb_ = BindToCurrentLoop(init_cb);

  client_ = client;
  wall_clock_time_cb_ = wall_clock_time_cb;
  state_ = kInitializing;

  video_frame_stream_->Initialize(
      stream, base::Bind(&VideoRendererImpl::OnVideoFrameStreamInitialized,
                         weak_factory_.GetWeakPtr()),
      cdm_context, base::Bind(&VideoRendererImpl::OnStatisticsUpdate,
                              weak_factory_.GetWeakPtr()),
      base::Bind(&VideoRendererImpl::OnWaitingForDecryptionKey,
                 weak_factory_.GetWeakPtr()));
}

scoped_refptr<VideoFrame> VideoRendererImpl::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    bool background_rendering) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> result =
      algorithm_->Render(deadline_min, deadline_max, &frames_dropped);

  // Due to how the |algorithm_| holds frames, this should never be null if
  // we've had a proper startup sequence.
  DCHECK(result);

  // Declare HAVE_NOTHING if we reach a state where we can't progress playback
  // any further.  We don't want to do this if we've already done so, reached
  // end of stream, or have frames available.  We also don't want to do this in
  // background rendering mode unless this isn't the first background render
  // tick and we haven't seen any decoded frames since the last one.
  MaybeFireEndedCallback_Locked(true);
  if (buffering_state_ == BUFFERING_HAVE_ENOUGH && !received_end_of_stream_ &&
      !algorithm_->effective_frames_queued() &&
      (!background_rendering ||
       (!frames_decoded_ && was_background_rendering_))) {
    // Do not set |buffering_state_| here as the lock in FrameReady() may be
    // held already and it fire the state changes in the wrong order.
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&VideoRendererImpl::TransitionToHaveNothing,
                              weak_factory_.GetWeakPtr()));
  }

  // We don't count dropped frames in the background to avoid skewing the count
  // and impacting JavaScript visible metrics used by web developers.
  //
  // Just after resuming from background rendering, we also don't count the
  // dropped frames since they are likely just dropped due to being too old.
  if (!background_rendering && !was_background_rendering_)
    frames_dropped_ += frames_dropped;
  UpdateStats_Locked();
  was_background_rendering_ = background_rendering;

  // Always post this task, it will acquire new frames if necessary and since it
  // happens on another thread, even if we don't have room in the queue now, by
  // the time it runs (may be delayed up to 50ms for complex decodes!) we might.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoRendererImpl::AttemptReadAndCheckForMetadataChanges,
                 weak_factory_.GetWeakPtr(), result->format(),
                 result->natural_size()));

  return result;
}

void VideoRendererImpl::OnFrameDropped() {
  base::AutoLock auto_lock(lock_);
  algorithm_->OnLastFrameDropped();
}

void VideoRendererImpl::OnVideoFrameStreamInitialized(bool success) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
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

  algorithm_.reset(new VideoRendererAlgorithm(wall_clock_time_cb_));
  if (!drop_frames_)
    algorithm_->disable_frame_dropping();

  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void VideoRendererImpl::OnPlaybackError(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnError(error);
}

void VideoRendererImpl::OnPlaybackEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnEnded();
}

void VideoRendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void VideoRendererImpl::OnBufferingStateChange(BufferingState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnBufferingStateChange(state);
}

void VideoRendererImpl::OnWaitingForDecryptionKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaitingForDecryptionKey();
}

void VideoRendererImpl::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
}

void VideoRendererImpl::SetGpuMemoryBufferVideoForTesting(
    std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool) {
  gpu_memory_buffer_pool_.swap(gpu_memory_buffer_pool);
}

void VideoRendererImpl::OnTimeStateChanged(bool time_progressing) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  time_progressing_ = time_progressing;

  // WARNING: Do not attempt to use |lock_| here as this may be a reentrant call
  // in response to callbacks firing above.

  if (sink_started_ == time_progressing_)
    return;

  if (time_progressing_) {
    // If only an EOS frame came in after a seek, the renderer may not have
    // received the ended event yet though we've posted it.
    if (rendered_end_of_stream_)
      return;

    // If we have no frames queued, there is a pending buffering state change in
    // flight and we should ignore the start attempt.
    if (!algorithm_->frames_queued()) {
      DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
      return;
    }

    StartSink();
  } else {
    StopSink();

    // Make sure we expire everything we can if we can't read anymore currently,
    // otherwise playback may hang indefinitely.  Note: There are no effective
    // frames queued at this point, otherwise FrameReady() would have canceled
    // the underflow state before reaching this point.
    if (buffering_state_ == BUFFERING_HAVE_NOTHING)
      RemoveFramesForUnderflowOrBackgroundRendering();
  }
}

void VideoRendererImpl::FrameReadyForCopyingToGpuMemoryBuffers(
    VideoFrameStream::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  if (status != VideoFrameStream::OK || IsBeforeStartTime(frame->timestamp())) {
    VideoRendererImpl::FrameReady(status, frame);
    return;
  }

  DCHECK(frame);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      frame, base::Bind(&VideoRendererImpl::FrameReady,
                        frame_callback_weak_factory_.GetWeakPtr(), status));
}

void VideoRendererImpl::FrameReady(VideoFrameStream::Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kFlushed);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == VideoFrameStream::DECODE_ERROR) {
    DCHECK(!frame);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoRendererImpl::OnPlaybackError,
                   weak_factory_.GetWeakPtr(), PIPELINE_ERROR_DECODE));
    return;
  }

  // Already-queued VideoFrameStream ReadCB's can fire after various state
  // transitions have happened; in that case just drop those frames
  // immediately.
  if (state_ == kFlushing)
    return;

  DCHECK_EQ(state_, kPlaying);

  // Can happen when demuxers are preparing for a new Seek().
  if (!frame) {
    DCHECK_EQ(status, VideoFrameStream::DEMUXER_READ_ABORTED);
    return;
  }

  if (frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM)) {
    DCHECK(!received_end_of_stream_);
    received_end_of_stream_ = true;
  } else if ((low_delay_ || !video_frame_stream_->CanReadWithoutStalling()) &&
             IsBeforeStartTime(frame->timestamp())) {
    // Don't accumulate frames that are earlier than the start time if we
    // won't have a chance for a better frame, otherwise we could declare
    // HAVE_ENOUGH_DATA and start playback prematurely.
    AttemptRead_Locked();
    return;
  } else {
    // If the sink hasn't been started, we still have time to release less
    // than ideal frames prior to startup.  We don't use IsBeforeStartTime()
    // here since it's based on a duration estimate and we can be exact here.
    if (!sink_started_ && frame->timestamp() <= start_timestamp_)
      algorithm_->Reset();

    AddReadyFrame_Locked(frame);
  }

  // Attempt to purge bad frames in case of underflow or backgrounding.
  RemoveFramesForUnderflowOrBackgroundRendering();

  // We may have removed all frames above and have reached end of stream.
  MaybeFireEndedCallback_Locked(time_progressing_);

  // Paint the first frame if possible and necessary. PaintSingleFrame() will
  // ignore repeated calls for the same frame. Paint ahead of HAVE_ENOUGH_DATA
  // to ensure the user sees the frame as early as possible.
  if (!sink_started_ && algorithm_->frames_queued()) {
    // We want to paint the first frame under two conditions: Either (1) we have
    // enough frames to know it's definitely the first frame or (2) there may be
    // no more frames coming (sometimes unless we paint one of them).
    //
    // For the first condition, we need at least two frames or the first frame
    // must have a timestamp >= |start_timestamp_|, since otherwise we may be
    // prerolling frames before the actual start time that will be dropped.
    if (algorithm_->frames_queued() > 1 || received_end_of_stream_ ||
        algorithm_->first_frame()->timestamp() >= start_timestamp_ ||
        low_delay_ || !video_frame_stream_->CanReadWithoutStalling()) {
      scoped_refptr<VideoFrame> frame = algorithm_->first_frame();
      CheckForMetadataChanges(frame->format(), frame->natural_size());
      sink_->PaintSingleFrame(frame);
    }
  }

  // Signal buffering state if we've met our conditions.
  if (buffering_state_ == BUFFERING_HAVE_NOTHING && HaveEnoughData_Locked())
    TransitionToHaveEnough_Locked();

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
  AttemptRead_Locked();
}

bool VideoRendererImpl::HaveEnoughData_Locked() {
  DCHECK_EQ(state_, kPlaying);
  lock_.AssertAcquired();

  if (received_end_of_stream_)
    return true;

  if (HaveReachedBufferingCap())
    return true;

  if (was_background_rendering_ && frames_decoded_)
    return true;

  if (!low_delay_ && video_frame_stream_->CanReadWithoutStalling())
    return false;

  return algorithm_->effective_frames_queued() > 0;
}

void VideoRendererImpl::TransitionToHaveEnough_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  lock_.AssertAcquired();

  buffering_state_ = BUFFERING_HAVE_ENOUGH;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoRendererImpl::OnBufferingStateChange,
                            weak_factory_.GetWeakPtr(), buffering_state_));
}

void VideoRendererImpl::TransitionToHaveNothing() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (buffering_state_ != BUFFERING_HAVE_ENOUGH || HaveEnoughData_Locked())
    return;

  buffering_state_ = BUFFERING_HAVE_NOTHING;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoRendererImpl::OnBufferingStateChange,
                            weak_factory_.GetWeakPtr(), buffering_state_));
}

void VideoRendererImpl::AddReadyFrame_Locked(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  DCHECK(!frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));

  frames_decoded_++;

  algorithm_->EnqueueFrame(frame);
}

void VideoRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (pending_read_ || received_end_of_stream_)
    return;

  if (HaveReachedBufferingCap())
    return;

  switch (state_) {
    case kPlaying:
      pending_read_ = true;
      if (gpu_memory_buffer_pool_) {
        video_frame_stream_->Read(base::Bind(
            &VideoRendererImpl::FrameReadyForCopyingToGpuMemoryBuffers,
            frame_callback_weak_factory_.GetWeakPtr()));
      } else {
        video_frame_stream_->Read(
            base::Bind(&VideoRendererImpl::FrameReady,
                       frame_callback_weak_factory_.GetWeakPtr()));
      }
      return;
    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return;
  }
}

void VideoRendererImpl::OnVideoFrameStreamResetDone() {
  // We don't need to acquire the |lock_| here, because we can only get here
  // when Flush is in progress, so rendering and video sink must be stopped.
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!sink_started_);
  DCHECK_EQ(kFlushing, state_);
  DCHECK(!received_end_of_stream_);
  DCHECK(!rendered_end_of_stream_);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  // Pending read might be true if an async video frame copy is in flight.
  pending_read_ = false;
  // Drop any pending calls to FrameReady() and
  // FrameReadyForCopyingToGpuMemoryBuffers()
  frame_callback_weak_factory_.InvalidateWeakPtrs();
  state_ = kFlushed;
  base::ResetAndReturn(&flush_cb_).Run();
}

void VideoRendererImpl::UpdateStats_Locked() {
  lock_.AssertAcquired();
  DCHECK_GE(frames_decoded_, 0);
  DCHECK_GE(frames_dropped_, 0);

  if (frames_decoded_ || frames_dropped_) {
    PipelineStatistics statistics;
    statistics.video_frames_decoded = frames_decoded_;
    statistics.video_frames_dropped = frames_dropped_;

    const size_t memory_usage = algorithm_->GetMemoryUsage();
    statistics.video_memory_usage = memory_usage - last_video_memory_usage_;

    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&VideoRendererImpl::OnStatisticsUpdate,
                                      weak_factory_.GetWeakPtr(), statistics));
    frames_decoded_ = 0;
    frames_dropped_ = 0;
    last_video_memory_usage_ = memory_usage;
  }
}

bool VideoRendererImpl::HaveReachedBufferingCap() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  const size_t kMaxVideoFrames = limits::kMaxVideoFrames;

  // When the display rate is less than the frame rate, the effective frames
  // queued may be much smaller than the actual number of frames queued.  Here
  // we ensure that frames_queued() doesn't get excessive.
  return algorithm_->effective_frames_queued() >= kMaxVideoFrames ||
         algorithm_->frames_queued() >= 3 * kMaxVideoFrames;
}

void VideoRendererImpl::StartSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GT(algorithm_->frames_queued(), 0u);
  sink_started_ = true;
  was_background_rendering_ = false;
  sink_->Start(this);
}

void VideoRendererImpl::StopSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  sink_->Stop();
  algorithm_->set_time_stopped();
  sink_started_ = false;
  was_background_rendering_ = false;
}

void VideoRendererImpl::MaybeFireEndedCallback_Locked(bool time_progressing) {
  lock_.AssertAcquired();

  // If there's only one frame in the video or Render() was never called, the
  // algorithm will have one frame linger indefinitely.  So in cases where the
  // frame duration is unknown and we've received EOS, fire it once we get down
  // to a single frame.

  // Don't fire ended if we haven't received EOS or have already done so.
  if (!received_end_of_stream_ || rendered_end_of_stream_)
    return;

  // Don't fire ended if time isn't moving and we have frames.
  if (!time_progressing && algorithm_->frames_queued())
    return;

  // Fire ended if we have no more effective frames or only ever had one frame.
  if (!algorithm_->effective_frames_queued() ||
      (algorithm_->frames_queued() == 1u &&
       algorithm_->average_frame_duration().is_zero())) {
    rendered_end_of_stream_ = true;
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&VideoRendererImpl::OnPlaybackEnded,
                                      weak_factory_.GetWeakPtr()));
  }
}

base::TimeTicks VideoRendererImpl::ConvertMediaTimestamp(
    base::TimeDelta media_time) {
  std::vector<base::TimeDelta> media_times(1, media_time);
  std::vector<base::TimeTicks> wall_clock_times;
  if (!wall_clock_time_cb_.Run(media_times, &wall_clock_times))
    return base::TimeTicks();
  return wall_clock_times[0];
}

base::TimeTicks VideoRendererImpl::GetCurrentMediaTimeAsWallClockTime() {
  std::vector<base::TimeTicks> current_time;
  wall_clock_time_cb_.Run(std::vector<base::TimeDelta>(), &current_time);
  return current_time[0];
}

bool VideoRendererImpl::IsBeforeStartTime(base::TimeDelta timestamp) {
  return timestamp + video_frame_stream_->AverageDuration() < start_timestamp_;
}

void VideoRendererImpl::RemoveFramesForUnderflowOrBackgroundRendering() {
  // Nothing to do if we're not underflowing, background rendering, or frame
  // dropping is disabled (test only).
  const bool have_nothing = buffering_state_ == BUFFERING_HAVE_NOTHING;
  if (!was_background_rendering_ && !have_nothing && !drop_frames_)
    return;

  // If there are no frames to remove, nothing can be done.
  if (!algorithm_->frames_queued())
    return;

  // If we're paused for prerolling (current time is 0), don't expire any
  // frames. It's possible that during preroll |have_nothing| is false while
  // |was_background_rendering_| is true. We differentiate this from actual
  // background rendering by checking if current time is 0.
  const base::TimeTicks current_time = GetCurrentMediaTimeAsWallClockTime();
  if (current_time.is_null())
    return;

  // Background rendering updates may not be ticking fast enough to remove
  // expired frames, so provide a boost here by ensuring we don't exit the
  // decoding cycle too early. Dropped frames are not counted in this case.
  if (was_background_rendering_) {
    algorithm_->RemoveExpiredFrames(tick_clock_->NowTicks());
    return;
  }

  // Use the current media wall clock time plus the frame duration since
  // RemoveExpiredFrames() is expecting the end point of an interval (it will
  // subtract from the given value). It's important to always call this so
  // that frame statistics are updated correctly.
  frames_dropped_ += algorithm_->RemoveExpiredFrames(
      current_time + algorithm_->average_frame_duration());

  // If we've paused for underflow, and still have no effective frames, clear
  // the entire queue.  Note: this may cause slight inaccuracies in the number
  // of dropped frames since the frame may have been rendered before.
  if (!sink_started_ && !algorithm_->effective_frames_queued()) {
    frames_dropped_ += algorithm_->frames_queued();
    algorithm_->Reset(
        VideoRendererAlgorithm::ResetFlag::kPreserveNextFrameEstimates);
  }
}

void VideoRendererImpl::CheckForMetadataChanges(VideoPixelFormat pixel_format,
                                                const gfx::Size& natural_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Notify client of size and opacity changes if this is the first frame
  // or if those have changed from the last frame.
  if (!have_renderered_frames_ || last_frame_natural_size_ != natural_size) {
    last_frame_natural_size_ = natural_size;
    client_->OnVideoNaturalSizeChange(last_frame_natural_size_);
  }

  const bool is_opaque = IsOpaque(pixel_format);
  if (!have_renderered_frames_ || last_frame_opaque_ != is_opaque) {
    last_frame_opaque_ = is_opaque;
    client_->OnVideoOpacityChange(last_frame_opaque_);
  }

  have_renderered_frames_ = true;
}

void VideoRendererImpl::AttemptReadAndCheckForMetadataChanges(
    VideoPixelFormat pixel_format,
    const gfx::Size& natural_size) {
  base::AutoLock auto_lock(lock_);
  CheckForMetadataChanges(pixel_format, natural_size);
  AttemptRead_Locked();
}

}  // namespace media
