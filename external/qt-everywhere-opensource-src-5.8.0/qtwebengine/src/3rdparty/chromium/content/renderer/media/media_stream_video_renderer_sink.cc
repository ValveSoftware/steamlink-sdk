// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_renderer_sink.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

const int kMinFrameSize = 2;

namespace content {

MediaStreamVideoRendererSink::MediaStreamVideoRendererSink(
    const blink::WebMediaStreamTrack& video_track,
    const base::Closure& error_cb,
    const RepaintCB& repaint_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : error_cb_(error_cb),
      repaint_cb_(repaint_cb),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      state_(STOPPED),
      frame_size_(kMinFrameSize, kMinFrameSize),
      video_track_(video_track),
      media_task_runner_(media_task_runner),
      weak_factory_(this) {
  if (gpu_factories &&
      gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames()) {
    gpu_memory_buffer_pool_.reset(new media::GpuMemoryBufferVideoFramePool(
        media_task_runner, worker_task_runner, gpu_factories));
  }
}

MediaStreamVideoRendererSink::~MediaStreamVideoRendererSink() {
  if (gpu_memory_buffer_pool_) {
    media_task_runner_->DeleteSoon(FROM_HERE,
                                   gpu_memory_buffer_pool_.release());
  }
}

void MediaStreamVideoRendererSink::Start() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STOPPED);

  MediaStreamVideoSink::ConnectToTrack(
      video_track_, media::BindToCurrentLoop(
                        base::Bind(&MediaStreamVideoRendererSink::OnVideoFrame,
                                   weak_factory_.GetWeakPtr())),
      // Local display video rendering is considered a secure link.
      true);
  state_ = STARTED;

  if (video_track_.source().getReadyState() ==
          blink::WebMediaStreamSource::ReadyStateEnded ||
      !video_track_.isEnabled()) {
    RenderSignalingFrame();
  }
}

void MediaStreamVideoRendererSink::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STARTED || state_ == PAUSED);
  MediaStreamVideoSink::DisconnectFromTrack();
  weak_factory_.InvalidateWeakPtrs();
  state_ = STOPPED;
  frame_size_.set_width(kMinFrameSize);
  frame_size_.set_height(kMinFrameSize);
}

void MediaStreamVideoRendererSink::Resume() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == PAUSED)
    state_ = STARTED;
}

void MediaStreamVideoRendererSink::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == STARTED)
    state_ = PAUSED;
}

void MediaStreamVideoRendererSink::SetGpuMemoryBufferVideoForTesting(
    media::GpuMemoryBufferVideoFramePool* gpu_memory_buffer_pool) {
  gpu_memory_buffer_pool_.reset(gpu_memory_buffer_pool);
}

void MediaStreamVideoRendererSink::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state == blink::WebMediaStreamSource::ReadyStateEnded)
    RenderSignalingFrame();
}

void MediaStreamVideoRendererSink::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(frame);
  if (state_ != STARTED)
    return;

  if (!gpu_memory_buffer_pool_) {
    FrameReady(frame);
    return;
  }

  //  |gpu_memory_buffer_pool_| deletion is going to be posted to
  //  |media_task_runner_|. base::Unretained() usage is fine since
  //  |gpu_memory_buffer_pool_| outlives the task.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &media::GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame,
          base::Unretained(gpu_memory_buffer_pool_.get()), frame,
          media::BindToCurrentLoop(
              base::Bind(&MediaStreamVideoRendererSink::FrameReady,
                         weak_factory_.GetWeakPtr()))));
}

void MediaStreamVideoRendererSink::FrameReady(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(frame);

  frame_size_ = frame->natural_size();
  TRACE_EVENT_INSTANT1("media_stream_video_renderer_sink", "FrameReady",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       frame->timestamp().InMilliseconds());
  repaint_cb_.Run(frame);
}

void MediaStreamVideoRendererSink::RenderSignalingFrame() {
  // This is necessary to make sure audio can play if the video tag src is
  // a MediaStream video track that has been rejected or ended.
  // It also ensure that the renderer don't hold a reference to a real video
  // frame if no more frames are provided. This is since there might be a
  // finite number of available buffers. E.g, video that
  // originates from a video camera.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size_);
  video_frame->metadata()->SetBoolean(media::VideoFrameMetadata::END_OF_STREAM,
                                      true);
  video_frame->metadata()->SetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, base::TimeTicks::Now());
  OnVideoFrame(video_frame, base::TimeTicks());
}

}  // namespace content
