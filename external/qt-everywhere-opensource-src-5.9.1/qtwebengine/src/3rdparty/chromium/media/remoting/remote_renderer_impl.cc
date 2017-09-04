// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remote_renderer_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/media_keys.h"
#include "media/remoting/remote_demuxer_stream_adapter.h"
#include "media/remoting/remoting_renderer_controller.h"
#include "media/remoting/rpc/proto_enum_utils.h"
#include "media/remoting/rpc/proto_utils.h"

namespace media {

RemoteRendererImpl::RemoteRendererImpl(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    const base::WeakPtr<RemotingRendererController>&
        remoting_renderer_controller,
    VideoRendererSink* video_renderer_sink)
    : state_(STATE_UNINITIALIZED),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_task_runner_(std::move(media_task_runner)),
      demuxer_stream_provider_(nullptr),
      client_(nullptr),
      remoting_renderer_controller_(remoting_renderer_controller),
      rpc_broker_(remoting_renderer_controller_->GetRpcBroker()),
      rpc_handle_(remoting::RpcBroker::GetUniqueHandle()),
      remote_renderer_handle_(remoting::kInvalidHandle),
      interstitial_ui_(video_renderer_sink,
                       remoting_renderer_controller->pipeline_metadata()),
      weak_factory_(this) {
  VLOG(2) << __FUNCTION__;
  // The constructor is running on the main thread.
  DCHECK(remoting_renderer_controller);

  UpdateInterstitial();

  const remoting::RpcBroker::ReceiveMessageCallback receive_callback =
      base::Bind(&RemoteRendererImpl::OnMessageReceivedOnMainThread,
                 media_task_runner_, weak_factory_.GetWeakPtr());
  rpc_broker_->RegisterMessageReceiverCallback(rpc_handle_, receive_callback);
}

RemoteRendererImpl::~RemoteRendererImpl() {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  // Post task on main thread to unregister message receiver.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&remoting::RpcBroker::UnregisterMessageReceiverCallback,
                 rpc_broker_, rpc_handle_));
}

void RemoteRendererImpl::Initialize(
    DemuxerStreamProvider* demuxer_stream_provider,
    media::RendererClient* client,
    const PipelineStatusCB& init_cb) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream_provider);
  DCHECK(client);
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  demuxer_stream_provider_ = demuxer_stream_provider;
  client_ = client;
  init_workflow_done_callback_ = init_cb;

  state_ = STATE_CREATE_PIPE;
  // Create audio mojo data pipe handles if audio is available.
  ::media::DemuxerStream* audio_demuxer_stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::AUDIO);
  std::unique_ptr<mojo::DataPipe> audio_data_pipe;
  if (audio_demuxer_stream) {
    audio_data_pipe = base::WrapUnique(remoting::CreateDataPipe());
  }

  // Create video mojo data pipe handles if video is available.
  ::media::DemuxerStream* video_demuxer_stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::VIDEO);
  std::unique_ptr<mojo::DataPipe> video_data_pipe;
  if (video_demuxer_stream) {
    video_data_pipe = base::WrapUnique(remoting::CreateDataPipe());
  }

  // Establish remoting data pipe connection using main thread.
  const RemotingSourceImpl::DataPipeStartCallback data_pipe_callback =
      base::Bind(&RemoteRendererImpl::OnDataPipeCreatedOnMainThread,
                 media_task_runner_, weak_factory_.GetWeakPtr());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RemotingRendererController::StartDataPipe,
                 remoting_renderer_controller_, base::Passed(&audio_data_pipe),
                 base::Passed(&video_data_pipe), data_pipe_callback));
}

void RemoteRendererImpl::SetCdm(CdmContext* cdm_context,
                                const CdmAttachedCB& cdm_attached_cb) {
  VLOG(2) << __FUNCTION__ << " cdm_id:" << cdm_context->GetCdmId();
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // TODO(erickung): add implementation once Remote CDM implementation is done.
  // Right now it returns callback immediately.
  if (!cdm_attached_cb.is_null()) {
    cdm_attached_cb.Run(false);
  }
}

void RemoteRendererImpl::Flush(const base::Closure& flush_cb) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(flush_cb_.is_null());

  if (state_ != STATE_PLAYING) {
    DCHECK_EQ(state_, STATE_ERROR);
    return;
  }

  state_ = STATE_FLUSHING;
  base::Optional<uint32_t> flush_audio_count;
  if (audio_demuxer_stream_adapter_)
    flush_audio_count = audio_demuxer_stream_adapter_->SignalFlush(true);
  base::Optional<uint32_t> flush_video_count;
  if (video_demuxer_stream_adapter_)
    flush_video_count = video_demuxer_stream_adapter_->SignalFlush(true);
  // Makes sure flush count is valid if stream is available or both audio and
  // video agrees on the same flushing state.
  if ((audio_demuxer_stream_adapter_ && !flush_audio_count.has_value()) ||
      (video_demuxer_stream_adapter_ && !flush_video_count.has_value()) ||
      (audio_demuxer_stream_adapter_ && video_demuxer_stream_adapter_ &&
       flush_audio_count.has_value() != flush_video_count.has_value())) {
    VLOG(1) << "Ignoring flush request while under flushing operation";
    return;
  }

  flush_cb_ = flush_cb;

  // Issues RPC_R_FLUSHUNTIL RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_R_FLUSHUNTIL);
  remoting::pb::RendererFlushUntil* message =
      rpc->mutable_renderer_flushuntil_rpc();
  if (flush_audio_count.has_value())
    message->set_audio_count(*flush_audio_count);
  if (flush_video_count.has_value())
    message->set_video_count(*flush_video_count);
  message->set_callback_handle(rpc_handle_);
  SendRpcToRemote(std::move(rpc));
}

void RemoteRendererImpl::StartPlayingFrom(base::TimeDelta time) {
  VLOG(2) << __FUNCTION__ << ": " << time.InMicroseconds();
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (state_ != STATE_PLAYING) {
    DCHECK_EQ(state_, STATE_ERROR);
    return;
  }

  // Issues RPC_R_STARTPLAYINGFROM RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_R_STARTPLAYINGFROM);
  rpc->set_integer64_value(time.InMicroseconds());
  SendRpcToRemote(std::move(rpc));

  {
    base::AutoLock auto_lock(time_lock_);
    current_media_time_ = time;
  }
}

void RemoteRendererImpl::SetPlaybackRate(double playback_rate) {
  VLOG(2) << __FUNCTION__ << ": " << playback_rate;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (state_ != STATE_PLAYING)
    return;

  // Issues RPC_R_SETPLAYBACKRATE RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_R_SETPLAYBACKRATE);
  rpc->set_double_value(playback_rate);
  SendRpcToRemote(std::move(rpc));
}

void RemoteRendererImpl::SetVolume(float volume) {
  VLOG(2) << __FUNCTION__ << ": " << volume;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Issues RPC_R_SETVOLUME RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_R_SETVOLUME);
  rpc->set_double_value(volume);
  SendRpcToRemote(std::move(rpc));
}

base::TimeDelta RemoteRendererImpl::GetMediaTime() {
  // No BelongsToCurrentThread() checking because this can be called from other
  // threads.
  // TODO(erickung): Interpolate current media time using local system time.
  // Current receiver is to update |current_media_time_| every 250ms. But it
  // needs to lower the update frequency in order to reduce network usage. Hence
  // the interpolation is needed after receiver implementation is changed.
  base::AutoLock auto_lock(time_lock_);
  return current_media_time_;
}

bool RemoteRendererImpl::HasAudio() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return audio_demuxer_stream_adapter_ ? true : false;
}

bool RemoteRendererImpl::HasVideo() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return video_demuxer_stream_adapter_ ? true : false;
}

// static
void RemoteRendererImpl::OnDataPipeCreatedOnMainThread(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    base::WeakPtr<RemoteRendererImpl> self,
    mojom::RemotingDataStreamSenderPtrInfo audio,
    mojom::RemotingDataStreamSenderPtrInfo video,
    mojo::ScopedDataPipeProducerHandle audio_handle,
    mojo::ScopedDataPipeProducerHandle video_handle) {
  media_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RemoteRendererImpl::OnDataPipeCreated, self,
                 base::Passed(&audio), base::Passed(&video),
                 base::Passed(&audio_handle), base::Passed(&video_handle)));
}

void RemoteRendererImpl::OnDataPipeCreated(
    mojom::RemotingDataStreamSenderPtrInfo audio,
    mojom::RemotingDataStreamSenderPtrInfo video,
    mojo::ScopedDataPipeProducerHandle audio_handle,
    mojo::ScopedDataPipeProducerHandle video_handle) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!init_workflow_done_callback_.is_null());
  DCHECK_EQ(state_, STATE_CREATE_PIPE);

  // Create audio demuxer stream adapter if audio is available.
  ::media::DemuxerStream* audio_demuxer_stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::AUDIO);
  if (audio_demuxer_stream && audio.is_valid() && audio_handle.is_valid()) {
    VLOG(2) << "Initialize audio";
    audio_demuxer_stream_adapter_.reset(
        new remoting::RemoteDemuxerStreamAdapter(
            main_task_runner_, media_task_runner_, "audio",
            audio_demuxer_stream, rpc_broker_, std::move(audio),
            std::move(audio_handle)));
  }

  // Create video demuxer stream adapter if video is available.
  ::media::DemuxerStream* video_demuxer_stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::VIDEO);
  if (video_demuxer_stream && video.is_valid() && video_handle.is_valid()) {
    VLOG(2) << "Initialize video";
    video_demuxer_stream_adapter_.reset(
        new remoting::RemoteDemuxerStreamAdapter(
            main_task_runner_, media_task_runner_, "video",
            video_demuxer_stream, rpc_broker_, std::move(video),
            std::move(video_handle)));
  }

  // Checks if data pipe is created successfully.
  if (!audio_demuxer_stream_adapter_ && !video_demuxer_stream_adapter_) {
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }

  state_ = STATE_ACQUIRING;
  // Issues RPC_ACQUIRE_RENDERER RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remoting::kReceiverHandle);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_ACQUIRE_RENDERER);
  rpc->set_integer_value(rpc_handle_);
  SendRpcToRemote(std::move(rpc));
}

// static
void RemoteRendererImpl::OnMessageReceivedOnMainThread(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    base::WeakPtr<RemoteRendererImpl> self,
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  media_task_runner->PostTask(
      FROM_HERE, base::Bind(&RemoteRendererImpl::OnReceivedRpc, self,
                            base::Passed(&message)));
}

void RemoteRendererImpl::OnReceivedRpc(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  VLOG(2) << __FUNCTION__ << ":" << message->proc();
  switch (message->proc()) {
    case remoting::pb::RpcMessage::RPC_ACQUIRE_RENDERER_DONE:
      AcquireRendererDone(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_R_INITIALIZE_CALLBACK:
      InitializeCallback(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK:
      FlushUntilCallback();
      break;
    case remoting::pb::RpcMessage::RPC_R_SETCDM_CALLBACK:
      SetCdmCallback(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONTIMEUPDATE:
      OnTimeUpdate(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE:
      OnBufferingStateChange(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONENDED:
      client_->OnEnded();
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONERROR:
      OnFatalError(PIPELINE_ERROR_DECODE);
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE:
      OnVideoNaturalSizeChange(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE:
      OnVideoOpacityChange(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONSTATISTICSUPDATE:
      OnStatisticsUpdate(std::move(message));
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONWAITINGFORDECRYPTIONKEY:
      client_->OnWaitingForDecryptionKey();
      break;
    case remoting::pb::RpcMessage::RPC_RC_ONDURATIONCHANGE:
      OnDurationChange(std::move(message));
      break;

    default:
      LOG(ERROR) << "Unknown rpc: " << message->proc();
  }
}

void RemoteRendererImpl::SendRpcToRemote(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(main_task_runner_);
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&remoting::RpcBroker::SendMessageToRemote,
                            rpc_broker_, base::Passed(&message)));
}

void RemoteRendererImpl::AcquireRendererDone(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  DCHECK(!init_workflow_done_callback_.is_null());
  if (state_ != STATE_ACQUIRING || init_workflow_done_callback_.is_null()) {
    VLOG(1) << "Unexpected acquire renderer done RPC. Shutting down.";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }
  remote_renderer_handle_ = message->integer_value();
  state_ = STATE_INITIALIZING;

  // Issues RPC_R_INITIALIZE RPC message to initialize renderer.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_R_INITIALIZE);
  remoting::pb::RendererInitialize* init =
      rpc->mutable_renderer_initialize_rpc();
  init->set_client_handle(rpc_handle_);
  init->set_audio_demuxer_handle(
      audio_demuxer_stream_adapter_
          ? audio_demuxer_stream_adapter_->rpc_handle()
          : remoting::kInvalidHandle);
  init->set_video_demuxer_handle(
      video_demuxer_stream_adapter_
          ? video_demuxer_stream_adapter_->rpc_handle()
          : remoting::kInvalidHandle);
  init->set_callback_handle(rpc_handle_);
  SendRpcToRemote(std::move(rpc));
}

void RemoteRendererImpl::InitializeCallback(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  DCHECK(!init_workflow_done_callback_.is_null());
  if (state_ != STATE_INITIALIZING || init_workflow_done_callback_.is_null()) {
    VLOG(1) << "Unexpected initialize callback RPC. Shutting down.";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }

  bool success = message->boolean_value();
  if (!success) {
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }

  state_ = STATE_PLAYING;
  base::ResetAndReturn(&init_workflow_done_callback_).Run(::media::PIPELINE_OK);
}

void RemoteRendererImpl::FlushUntilCallback() {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (state_ != STATE_FLUSHING || flush_cb_.is_null()) {
    VLOG(1) << "Unexpected flushuntil callback RPC. Shutting down.";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }
  state_ = STATE_PLAYING;
  if (audio_demuxer_stream_adapter_)
    audio_demuxer_stream_adapter_->SignalFlush(false);
  if (video_demuxer_stream_adapter_)
    video_demuxer_stream_adapter_->SignalFlush(false);
  base::ResetAndReturn(&flush_cb_).Run();
}

void RemoteRendererImpl::SetCdmCallback(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  // TODO(erickung): add implementation once Remote CDM implementation is done.
  NOTIMPLEMENTED();
}

void RemoteRendererImpl::OnTimeUpdate(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_ontimeupdate_rpc()) {
    VLOG(1) << __FUNCTION__ << " missing required RPC message";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }
  int64_t time_usec = message->rendererclient_ontimeupdate_rpc().time_usec();
  int64_t max_time_usec =
      message->rendererclient_ontimeupdate_rpc().max_time_usec();
  // Ignores invalid time, such as negative value, or time larger than max value
  // (usually the time stamp that all streams are pushed into AV pipeline).
  if (time_usec < 0 || max_time_usec < 0 || time_usec > max_time_usec)
    return;

  {
    // Updates current time information.
    base::AutoLock auto_lock(time_lock_);
    current_media_time_ = base::TimeDelta::FromMicroseconds(time_usec);
    current_max_time_ = base::TimeDelta::FromMicroseconds(max_time_usec);
  }
  VLOG(3) << __FUNCTION__ << " max time:" << current_max_time_.InMicroseconds()
          << " new time:" << current_media_time_.InMicroseconds();
}

void RemoteRendererImpl::OnBufferingStateChange(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  if (!message->has_rendererclient_onbufferingstatechange_rpc()) {
    VLOG(1) << __FUNCTION__ << " missing required RPC message";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }
  base::Optional<::media::BufferingState> state =
      remoting::ToMediaBufferingState(
          message->rendererclient_onbufferingstatechange_rpc().state());
  if (!state.has_value())
    return;
  client_->OnBufferingStateChange(state.value());
}

void RemoteRendererImpl::OnVideoNaturalSizeChange(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_onvideonatualsizechange_rpc()) {
    VLOG(1) << __FUNCTION__ << " missing required RPC message";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }
  const auto& size_change =
      message->rendererclient_onvideonatualsizechange_rpc();
  if (size_change.width() <= 0 || size_change.height() <= 0)
    return;
  client_->OnVideoNaturalSizeChange(
      gfx::Size(size_change.width(), size_change.height()));
}

void RemoteRendererImpl::OnVideoOpacityChange(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  client_->OnVideoOpacityChange(message->boolean_value());
}

void RemoteRendererImpl::OnStatisticsUpdate(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_onstatisticsupdate_rpc()) {
    VLOG(1) << __FUNCTION__ << " missing required RPC message";
    OnFatalError(PIPELINE_ERROR_ABORT);
    return;
  }
  const auto& rpc_message = message->rendererclient_onstatisticsupdate_rpc();
  ::media::PipelineStatistics stats;
  stats.audio_bytes_decoded = rpc_message.audio_bytes_decoded();
  stats.video_bytes_decoded = rpc_message.video_bytes_decoded();
  stats.video_frames_decoded = rpc_message.video_frames_decoded();
  stats.video_frames_dropped = rpc_message.video_frames_dropped();
  stats.audio_memory_usage = rpc_message.audio_memory_usage();
  stats.video_memory_usage = rpc_message.video_memory_usage();
  client_->OnStatisticsUpdate(stats);
}

void RemoteRendererImpl::OnDurationChange(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  VLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  if (message->integer64_value() < 0)
    return;
  client_->OnDurationChange(
      base::TimeDelta::FromMicroseconds(message->integer64_value()));
}

void RemoteRendererImpl::OnFatalError(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  // An error has already been delivered.
  if (state_ == STATE_ERROR)
    return;

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RemoteRendererImpl::UpdateInterstitial,
                            weak_factory_.GetWeakPtr()));

  const State old_state = state_;
  state_ = STATE_ERROR;

  if (!init_workflow_done_callback_.is_null()) {
    DCHECK(old_state == STATE_CREATE_PIPE || old_state == STATE_ACQUIRING ||
           old_state == STATE_INITIALIZING);
    base::ResetAndReturn(&init_workflow_done_callback_).Run(error);
    return;
  }

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();

  // After OnError() returns, the pipeline may destroy |this|.
  client_->OnError(error);
}

void RemoteRendererImpl::UpdateInterstitial() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  interstitial_ui_.ShowInterstitial(
      remoting_renderer_controller_->remoting_source()->state() ==
      RemotingSessionState::SESSION_STARTED);
}

}  // namespace media
