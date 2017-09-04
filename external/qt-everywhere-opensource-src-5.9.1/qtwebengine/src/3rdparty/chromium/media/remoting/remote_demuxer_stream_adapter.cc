// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remote_demuxer_stream_adapter.h"

#include "base/base64.h"
#include "base/bind.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/remoting/rpc/proto_enum_utils.h"
#include "media/remoting/rpc/proto_utils.h"

namespace media {
namespace remoting {

// static
mojo::DataPipe* CreateDataPipe() {
  // Capacity in bytes for Mojo data pipe.
  constexpr int kMojoDataPipeCapacityInBytes = 512 * 1024;

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_WRITE_DATA_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kMojoDataPipeCapacityInBytes;
  return new mojo::DataPipe(options);
}

RemoteDemuxerStreamAdapter::RemoteDemuxerStreamAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    const std::string& name,
    ::media::DemuxerStream* demuxer_stream,
    const base::WeakPtr<RpcBroker>& rpc_broker,
    mojom::RemotingDataStreamSenderPtrInfo stream_sender_info,
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : main_task_runner_(std::move(main_task_runner)),
      media_task_runner_(std::move(media_task_runner)),
      name_(name),
      rpc_broker_(rpc_broker),
      rpc_handle_(RpcBroker::GetUniqueHandle()),
      demuxer_stream_(demuxer_stream),
      type_(demuxer_stream ? demuxer_stream->type() : DemuxerStream::UNKNOWN),
      remote_callback_handle_(kInvalidHandle),
      read_until_count_(0),
      last_count_(0),
      processing_read_rpc_(false),
      pending_flush_(false),
      current_pending_frame_offset_(0),
      pending_frame_is_eos_(false),
      media_status_(::media::DemuxerStream::kOk),
      producer_handle_(std::move(producer_handle)),
      request_buffer_weak_factory_(this),
      weak_factory_(this) {
  DCHECK(main_task_runner_);
  DCHECK(media_task_runner_);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream);
  const RpcBroker::ReceiveMessageCallback receive_callback =
      media::BindToCurrentLoop(
          base::Bind(&RemoteDemuxerStreamAdapter::OnReceivedRpc,
                     weak_factory_.GetWeakPtr()));
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&remoting::RpcBroker::RegisterMessageReceiverCallback,
                 rpc_broker_, rpc_handle_, receive_callback));

  stream_sender_.Bind(std::move(stream_sender_info));
  stream_sender_.set_connection_error_handler(base::Bind(
      &RemoteDemuxerStreamAdapter::OnFatalError, weak_factory_.GetWeakPtr()));
}

RemoteDemuxerStreamAdapter::~RemoteDemuxerStreamAdapter() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&remoting::RpcBroker::UnregisterMessageReceiverCallback,
                 rpc_broker_, rpc_handle_));
}

base::Optional<uint32_t> RemoteDemuxerStreamAdapter::SignalFlush(
    bool flushing) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  VLOG(2) << __FUNCTION__ << " (" << name_ << "): " << flushing;

  // Ignores if |pending_flush_| states is same.
  if (pending_flush_ == flushing)
    return base::nullopt;

  // Cleans up pending frame data.
  pending_frame_.clear();
  current_pending_frame_offset_ = 0;
  pending_frame_is_eos_ = false;
  // Invalidates pending Read() tasks.
  request_buffer_weak_factory_.InvalidateWeakPtrs();

  // Cancels in flight data in browser process.
  pending_flush_ = flushing;
  if (flushing) {
    stream_sender_->CancelInFlightData();
  } else {
    processing_read_rpc_ = false;
  }
  return last_count_;
}

void RemoteDemuxerStreamAdapter::OnReceivedRpc(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  DCHECK(rpc_handle_ == message->handle());

  switch (message->proc()) {
    case remoting::pb::RpcMessage::RPC_DS_INITIALIZE:
      Initialize(message->integer_value());
      break;
    case remoting::pb::RpcMessage::RPC_DS_READUNTIL:
      ReadUntil(std::move(message));
      break;
    default:
      VLOG(2) << "Unknown RPC: " << message->proc();
  }
}

void RemoteDemuxerStreamAdapter::Initialize(int remote_callback_handle) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_flush_);
  VLOG(2) << __FUNCTION__ << " (" << name_ << "):" << type_;

  // Checks if initialization had been called or not.
  if (remote_callback_handle_ != kInvalidHandle) {
    VLOG(1) << "Duplicated initialization. Have: " << remote_callback_handle_
            << ", Given: " << remote_callback_handle;
    // Shuts down data pipe if available if providing different remote callback
    // handle for initialization. Otherwise, just silently ignore the duplicated
    // request.
    if (remote_callback_handle_ != remote_callback_handle) {
      OnFatalError();
    }
    return;
  }
  remote_callback_handle_ = remote_callback_handle;

  if (type_ == ::media::DemuxerStream::VIDEO)
    video_config_ = demuxer_stream_->video_decoder_config();
  else if (type_ == ::media::DemuxerStream::AUDIO)
    audio_config_ = demuxer_stream_->audio_decoder_config();

  // Issues RPC_DS_INITIALIZE_CALLBACK RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(remote_callback_handle_);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
  auto* init_cb_message = rpc->mutable_demuxerstream_initializecb_rpc();
  init_cb_message->set_type(type_);
  switch (type_) {
    case ::media::DemuxerStream::Type::AUDIO: {
      pb::AudioDecoderConfig* audio_message =
          init_cb_message->mutable_audio_decoder_config();
      ConvertAudioDecoderConfigToProto(audio_config_, audio_message);
      break;
    }
    case ::media::DemuxerStream::Type::VIDEO: {
      pb::VideoDecoderConfig* video_message =
          init_cb_message->mutable_video_decoder_config();
      ConvertVideoDecoderConfigToProto(video_config_, video_message);
      break;
    }
    default:
      NOTREACHED();
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&remoting::RpcBroker::SendMessageToRemote,
                            rpc_broker_, base::Passed(&rpc)));
}

void RemoteDemuxerStreamAdapter::ReadUntil(
    std::unique_ptr<remoting::pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  if (pending_flush_) {
    VLOG(2) << "Skip actions since it's in the flushing state";
    return;
  }

  if (processing_read_rpc_) {
    VLOG(2) << "Ignore read request while it's in the reading state.";
    return;
  }

  DCHECK(message->has_demuxerstream_readuntil_rpc());
  const pb::DemuxerStreamReadUntil rpc_message =
      message->demuxerstream_readuntil_rpc();
  if (rpc_message.count() <= last_count_) {
    VLOG(1) << "Request count shouldn't be smaller than or equal to current "
               "frame count";
    return;
  }

  processing_read_rpc_ = true;
  read_until_count_ = rpc_message.count();
  RequestBuffer(rpc_message.callback_handle());
}

void RemoteDemuxerStreamAdapter::RequestBuffer(int callback_handle) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(processing_read_rpc_);
  if (pending_flush_) {
    VLOG(2) << "Skip actions since it's in the flushing state";
    return;
  }
  demuxer_stream_->Read(base::Bind(&RemoteDemuxerStreamAdapter::OnNewBuffer,
                                   request_buffer_weak_factory_.GetWeakPtr(),
                                   callback_handle));
}

void RemoteDemuxerStreamAdapter::OnNewBuffer(
    int callback_handle,
    ::media::DemuxerStream::Status status,
    const scoped_refptr<::media::DecoderBuffer>& input) {
  VLOG(3) << __FUNCTION__ << " (" << name_ << ") status:" << status;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (pending_flush_) {
    VLOG(2) << "Skip actions since it's in the flushing state";
    return;
  }

  switch (status) {
    case ::media::DemuxerStream::kAborted:
      DCHECK(!input);
      SendReadAck(callback_handle);
      return;
    case ::media::DemuxerStream::kConfigChanged:
      // TODO(erickung): consider sending updated Audio/Video decoder config to
      // RemotingRendererController.
      // Stores available audio/video decoder config and issues
      // RPC_DS_READUNTIL_CALLBACK RPC to notify receiver.
      DCHECK(!input);
      media_status_ = status;
      if (demuxer_stream_->type() == ::media::DemuxerStream::VIDEO)
        video_config_ = demuxer_stream_->video_decoder_config();
      if (demuxer_stream_->type() == ::media::DemuxerStream::AUDIO)
        audio_config_ = demuxer_stream_->audio_decoder_config();
      SendReadAck(callback_handle);
      return;
    case ::media::DemuxerStream::kOk: {
      media_status_ = status;
      DCHECK(pending_frame_.empty());
      if (!producer_handle_.is_valid())
        return;  // Do not start sending (due to previous fatal error).
      pending_frame_ = DecoderBufferToByteArray(input);
      pending_frame_is_eos_ = input->end_of_stream();
      if (!write_watcher_.IsWatching()) {
        VLOG(2) << "Start Mojo data pipe watcher: " << name_;
        write_watcher_.Start(
            producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
            base::Bind(&RemoteDemuxerStreamAdapter::TryWriteData,
                       weak_factory_.GetWeakPtr(), callback_handle));
      }
      TryWriteData(callback_handle, MOJO_RESULT_OK);
    } break;
  }
}

void RemoteDemuxerStreamAdapter::TryWriteData(int callback_handle,
                                              MojoResult result) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (pending_frame_.empty()) {
    VLOG(3) << "No data available, waiting for demuxer";
    return;
  }

  DCHECK(processing_read_rpc_);
  if (!stream_sender_ || !producer_handle_.is_valid()) {
    VLOG(1) << "Ignore since data pipe stream sender is invalid";
    return;
  }

  uint32_t num_bytes = pending_frame_.size() - current_pending_frame_offset_;
  MojoResult mojo_result =
      WriteDataRaw(producer_handle_.get(),
                   pending_frame_.data() + current_pending_frame_offset_,
                   &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  if (mojo_result != MOJO_RESULT_OK) {
    if (mojo_result != MOJO_RESULT_SHOULD_WAIT) {
      VLOG(1) << "Pipe was closed unexpectedly (or a bug). result:"
              << mojo_result;
      OnFatalError();
    }
    return;
  }

  stream_sender_->ConsumeDataChunk(current_pending_frame_offset_, num_bytes,
                                   pending_frame_.size());
  current_pending_frame_offset_ += num_bytes;

  // Checks if all buffer was written to browser process.
  if (current_pending_frame_offset_ != pending_frame_.size()) {
    // Returns and wait for mojo watcher to notify to write more data.
    return;
  }

  // Signal mojo remoting service that all frame buffer is written to data pipe.
  stream_sender_->SendFrame();

  // Resets frame buffer variables.
  bool pending_frame_is_eos = pending_frame_is_eos_;
  ++last_count_;
  ResetPendingFrame();

  // Checks if it needs to send RPC_DS_READUNTIL_CALLBACK RPC message.
  if (read_until_count_ == last_count_ || pending_frame_is_eos) {
    SendReadAck(callback_handle);
    return;
  }

  // Contiune to read decoder buffer until reaching |read_until_count_| or
  // end of stream.
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RemoteDemuxerStreamAdapter::RequestBuffer,
                            weak_factory_.GetWeakPtr(), callback_handle));
}

void RemoteDemuxerStreamAdapter::SendReadAck(int callback_handle) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  VLOG(3) << __FUNCTION__ << " name:" << name_
          << " last frame id:" << last_count_
          << " remote_read_callback_handle:" << callback_handle
          << " media_status:" << media_status_;
  // Resets the flag that it's not in the ReadUntil process.
  processing_read_rpc_ = false;

  // Issues RPC_DS_READUNTIL_CALLBACK RPC message.
  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  rpc->set_handle(callback_handle);
  rpc->set_proc(remoting::pb::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  auto* message = rpc->mutable_demuxerstream_readuntilcb_rpc();
  message->set_count(last_count_);
  message->set_status(
      remoting::ToProtoDemuxerStreamStatus(media_status_).value());
  if (media_status_ == ::media::DemuxerStream::kConfigChanged) {
    if (audio_config_.IsValidConfig()) {
      pb::AudioDecoderConfig* audio_message =
          message->mutable_audio_decoder_config();
      ConvertAudioDecoderConfigToProto(audio_config_, audio_message);
    } else if (video_config_.IsValidConfig()) {
      pb::VideoDecoderConfig* video_message =
          message->mutable_video_decoder_config();
      ConvertVideoDecoderConfigToProto(video_config_, video_message);
    } else {
      NOTREACHED();
    }
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&remoting::RpcBroker::SendMessageToRemote,
                            rpc_broker_, base::Passed(&rpc)));

  // Resets audio/video decoder config since it only sends once.
  if (audio_config_.IsValidConfig())
    audio_config_ = ::media::AudioDecoderConfig();
  if (video_config_.IsValidConfig())
    video_config_ = ::media::VideoDecoderConfig();
}

void RemoteDemuxerStreamAdapter::ResetPendingFrame() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  current_pending_frame_offset_ = 0;
  pending_frame_.clear();
  pending_frame_is_eos_ = false;
}

void RemoteDemuxerStreamAdapter::OnFatalError() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  VLOG(2) << __FUNCTION__;
  // Resets mojo data pipe producer handle.
  producer_handle_.reset();

  // Resetting |stream_sender_| will close Mojo message pipe, which will cause
  // the entire remoting session to be shut down.
  if (stream_sender_) {
    VLOG(2) << "Reset data stream sender";
    stream_sender_.reset();
  }

  if (write_watcher_.IsWatching()) {
    VLOG(2) << "Cancel mojo data pipe watcher";
    write_watcher_.Cancel();
  }
}

}  // namespace remoting
}  // namespace media
