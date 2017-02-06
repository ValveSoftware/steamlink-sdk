// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/media_pipeline_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/common/media/cma_messages.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/renderer/media/audio_pipeline_proxy.h"
#include "chromecast/renderer/media/media_channel_proxy.h"
#include "chromecast/renderer/media/video_pipeline_proxy.h"

namespace chromecast {
namespace media {

// MediaPipelineProxyInternal -
// This class is not thread safe and should run on the same thread
// as the media channel proxy.
class MediaPipelineProxyInternal {
 public:
  static void Release(std::unique_ptr<MediaPipelineProxyInternal> proxy);

  explicit MediaPipelineProxyInternal(
      scoped_refptr<MediaChannelProxy> media_channel_proxy);
  virtual ~MediaPipelineProxyInternal();

  void SetClient(const MediaPipelineClient& client);
  void SetCdm(int render_frame_id, int cdm_id);
  void StartPlayingFrom(const base::TimeDelta& time);
  void Flush(const base::Closure& flush_cb);
  void Stop();
  void SetPlaybackRate(double playback_rate);

 private:
  void Shutdown();

  // Callbacks for CmaMessageFilterHost::MediaDelegate.
  void OnFlushDone();

  base::ThreadChecker thread_checker_;

  scoped_refptr<MediaChannelProxy> media_channel_proxy_;

  MediaPipelineClient client_;

  // Store the callback for a flush.
  base::Closure flush_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineProxyInternal);
};

// static
void MediaPipelineProxyInternal::Release(
    std::unique_ptr<MediaPipelineProxyInternal> proxy) {
  proxy->Shutdown();
}

MediaPipelineProxyInternal::MediaPipelineProxyInternal(
    scoped_refptr<MediaChannelProxy> media_channel_proxy)
    : media_channel_proxy_(media_channel_proxy) {
  DCHECK(media_channel_proxy.get());

  // Creation can be done on a different thread.
  thread_checker_.DetachFromThread();
}

MediaPipelineProxyInternal::~MediaPipelineProxyInternal() {
}

void MediaPipelineProxyInternal::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Remove any callback on VideoPipelineProxyInternal.
  media_channel_proxy_->SetMediaDelegate(
      CmaMessageFilterProxy::MediaDelegate());
}

void MediaPipelineProxyInternal::SetClient(const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client.error_cb.is_null());
  DCHECK(!client.buffering_state_cb.is_null());
  client_ = client;

  CmaMessageFilterProxy::MediaDelegate delegate;
  delegate.flush_cb = base::Bind(&MediaPipelineProxyInternal::OnFlushDone,
                                 base::Unretained(this));
  delegate.client = client;
  bool success = media_channel_proxy_->SetMediaDelegate(delegate);
  CHECK(success);
}

void MediaPipelineProxyInternal::SetCdm(int render_frame_id, int cdm_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool success = media_channel_proxy_->Send(
      std::unique_ptr<IPC::Message>(new CmaHostMsg_SetCdm(
          media_channel_proxy_->GetId(), render_frame_id, cdm_id)));
  LOG_IF(ERROR, !success) << "Failed to send SetCdm=" << cdm_id;
}

void MediaPipelineProxyInternal::Flush(const base::Closure& flush_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool success = media_channel_proxy_->Send(std::unique_ptr<IPC::Message>(
      new CmaHostMsg_Flush(media_channel_proxy_->GetId())));
  if (!success) {
    LOG(ERROR) << "Failed to send Flush";
    return;
  }
  DCHECK(flush_cb_.is_null());
  flush_cb_ = flush_cb;
}

void MediaPipelineProxyInternal::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool success = media_channel_proxy_->Send(std::unique_ptr<IPC::Message>(
      new CmaHostMsg_Stop(media_channel_proxy_->GetId())));
  if (!success)
    client_.error_cb.Run(::media::PIPELINE_ERROR_ABORT);
}

void MediaPipelineProxyInternal::StartPlayingFrom(const base::TimeDelta& time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool success = media_channel_proxy_->Send(std::unique_ptr<IPC::Message>(
      new CmaHostMsg_StartPlayingFrom(media_channel_proxy_->GetId(), time)));
  if (!success)
    client_.error_cb.Run(::media::PIPELINE_ERROR_ABORT);
}

void MediaPipelineProxyInternal::SetPlaybackRate(double playback_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_channel_proxy_->Send(
      std::unique_ptr<IPC::Message>(new CmaHostMsg_SetPlaybackRate(
          media_channel_proxy_->GetId(), playback_rate)));
}

void MediaPipelineProxyInternal::OnFlushDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!flush_cb_.is_null());
  base::ResetAndReturn(&flush_cb_).Run();
}

// A macro runs current member function on |io_task_runner_| thread.
#define FORWARD_ON_IO_THREAD(param_fn, ...)                        \
  io_task_runner_->PostTask(                                       \
      FROM_HERE, base::Bind(&MediaPipelineProxyInternal::param_fn, \
                            base::Unretained(proxy_.get()), ##__VA_ARGS__))

MediaPipelineProxy::MediaPipelineProxy(
    int render_frame_id,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    LoadType load_type)
    : io_task_runner_(io_task_runner),
      render_frame_id_(render_frame_id),
      media_channel_proxy_(new MediaChannelProxy),
      proxy_(new MediaPipelineProxyInternal(media_channel_proxy_)),
      has_audio_(false),
      has_video_(false),
      audio_pipeline_(
          new AudioPipelineProxy(io_task_runner, media_channel_proxy_)),
      video_pipeline_(
          new VideoPipelineProxy(io_task_runner, media_channel_proxy_)),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaChannelProxy::Open, media_channel_proxy_, load_type));
  thread_checker_.DetachFromThread();
}

MediaPipelineProxy::~MediaPipelineProxy() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineProxyInternal::Release, base::Passed(&proxy_)));
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaChannelProxy::Close, media_channel_proxy_));
}

void MediaPipelineProxy::SetClient(const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_IO_THREAD(SetClient, client);
}

void MediaPipelineProxy::SetCdm(int cdm_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_IO_THREAD(SetCdm, render_frame_id_, cdm_id);
}

AudioPipelineProxy* MediaPipelineProxy::GetAudioPipeline() const {
  return audio_pipeline_.get();
}

VideoPipelineProxy* MediaPipelineProxy::GetVideoPipeline() const {
  return video_pipeline_.get();
}

void MediaPipelineProxy::InitializeAudio(
    const ::media::AudioDecoderConfig& config,
    std::unique_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  has_audio_ = true;
  audio_pipeline_->Initialize(config, std::move(frame_provider), status_cb);
}

void MediaPipelineProxy::InitializeVideo(
    const std::vector<::media::VideoDecoderConfig>& configs,
    std::unique_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  has_video_ = true;
  video_pipeline_->Initialize(configs, std::move(frame_provider), status_cb);
}

void MediaPipelineProxy::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (has_audio_)
    audio_pipeline_->StartFeeding();
  if (has_video_)
    video_pipeline_->StartFeeding();
  FORWARD_ON_IO_THREAD(StartPlayingFrom, time);
}

void MediaPipelineProxy::Flush(const base::Closure& flush_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio_ || has_video_);

  ::media::SerialRunner::Queue bound_fns;
  if (has_audio_) {
    bound_fns.Push(base::Bind(&AudioPipelineProxy::Flush,
                              base::Unretained(audio_pipeline_.get())));
  }
  if (has_video_) {
    bound_fns.Push(base::Bind(&VideoPipelineProxy::Flush,
                              base::Unretained(video_pipeline_.get())));
  }
  ::media::PipelineStatusCB cb =
      base::Bind(&MediaPipelineProxy::OnProxyFlushDone, weak_this_, flush_cb);
  pending_flush_callbacks_ = ::media::SerialRunner::Run(bound_fns, cb);
}

void MediaPipelineProxy::OnProxyFlushDone(const base::Closure& flush_cb,
                                          ::media::PipelineStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(status, ::media::PIPELINE_OK);
  pending_flush_callbacks_.reset();
  FORWARD_ON_IO_THREAD(Flush, flush_cb);
}

void MediaPipelineProxy::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio_ || has_video_);

  // Cancel pending flush callbacks since we are about to stop/shutdown
  // audio/video pipelines. This will ensure A/V Flush won't happen in
  // stopped state.
  pending_flush_callbacks_.reset();

  if (has_audio_)
    audio_pipeline_->Stop();
  if (has_video_)
    video_pipeline_->Stop();

  FORWARD_ON_IO_THREAD(Stop);
}

void MediaPipelineProxy::SetPlaybackRate(double playback_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_IO_THREAD(SetPlaybackRate, playback_rate);
}

}  // namespace cma
}  // namespace chromecast
