// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_source_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/remoting/rpc/proto_utils.h"

namespace media {

RemotingSourceImpl::RemotingSourceImpl(
    mojom::RemotingSourceRequest source_request,
    mojom::RemoterPtr remoter)
    : rpc_broker_(base::Bind(&RemotingSourceImpl::SendMessageToSink,
                             base::Unretained(this))),
      binding_(this, std::move(source_request)),
      remoter_(std::move(remoter)) {
  DCHECK(remoter_);
}

RemotingSourceImpl::~RemotingSourceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!clients_.empty()) {
    Shutdown();
    clients_.clear();
  }
}

void RemotingSourceImpl::OnSinkAvailable() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == RemotingSessionState::SESSION_UNAVAILABLE)
    UpdateAndNotifyState(RemotingSessionState::SESSION_CAN_START);
}

void RemotingSourceImpl::OnSinkGone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == RemotingSessionState::SESSION_PERMANENTLY_STOPPED)
    return;
  if (state_ == RemotingSessionState::SESSION_CAN_START) {
    UpdateAndNotifyState(RemotingSessionState::SESSION_UNAVAILABLE);
    return;
  }
  if (state_ == RemotingSessionState::SESSION_STARTED ||
      state_ == RemotingSessionState::SESSION_STARTING) {
    VLOG(1) << "Sink is gone in a remoting session.";
    // Remoting is being stopped by Remoter.
    UpdateAndNotifyState(RemotingSessionState::SESSION_STOPPING);
  }
}

void RemotingSourceImpl::OnStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting started successively.";
  if (clients_.empty() ||
      state_ == RemotingSessionState::SESSION_PERMANENTLY_STOPPED ||
      state_ == RemotingSessionState::SESSION_STOPPING) {
    for (Client* client : clients_)
      client->OnStarted(false);
    return;
  }
  for (Client* client : clients_)
    client->OnStarted(true);
  state_ = RemotingSessionState::SESSION_STARTED;
}

void RemotingSourceImpl::OnStartFailed(mojom::RemotingStartFailReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Failed to start remoting:" << reason;
  for (Client* client : clients_)
    client->OnStarted(false);
  if (state_ == RemotingSessionState::SESSION_PERMANENTLY_STOPPED)
    return;
  state_ = RemotingSessionState::SESSION_UNAVAILABLE;
}

void RemotingSourceImpl::OnStopped(mojom::RemotingStopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Remoting stopped: " << reason;
  if (state_ == RemotingSessionState::SESSION_PERMANENTLY_STOPPED)
    return;
  RemotingSessionState state = RemotingSessionState::SESSION_UNAVAILABLE;
  UpdateAndNotifyState(state);
}

void RemotingSourceImpl::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<remoting::pb::RpcMessage> rpc(new remoting::pb::RpcMessage());
  if (!rpc->ParseFromArray(message.data(), message.size())) {
    LOG(ERROR) << "corrupted Rpc message";
    Shutdown();
    return;
  }
  rpc_broker_.ProcessMessageFromRemote(std::move(rpc));
}

void RemotingSourceImpl::UpdateAndNotifyState(RemotingSessionState state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == state)
    return;
  state_ = state;
  for (Client* client : clients_)
    client->OnSessionStateChanged();
}

void RemotingSourceImpl::StartRemoting(Client* client) {
  DCHECK(std::find(clients_.begin(), clients_.end(), client) != clients_.end());

  switch (state_) {
    case SESSION_CAN_START:
      remoter_->Start();
      UpdateAndNotifyState(RemotingSessionState::SESSION_STARTING);
      break;
    case SESSION_STARTING:
      break;
    case SESSION_STARTED:
      client->OnStarted(true);
      break;
    case SESSION_STOPPING:
    case SESSION_UNAVAILABLE:
    case SESSION_PERMANENTLY_STOPPED:
      client->OnStarted(false);
      break;
  }
}

void RemotingSourceImpl::StopRemoting(Client* client) {
  DCHECK(std::find(clients_.begin(), clients_.end(), client) != clients_.end());

  VLOG(1) << "RemotingSourceImpl::StopRemoting: " << state_;

  if (state_ != RemotingSessionState::SESSION_STARTING &&
      state_ != RemotingSessionState::SESSION_STARTED)
    return;

  remoter_->Stop(mojom::RemotingStopReason::LOCAL_PLAYBACK);
  UpdateAndNotifyState(RemotingSessionState::SESSION_STOPPING);
}

void RemotingSourceImpl::AddClient(Client* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find(clients_.begin(), clients_.end(), client) == clients_.end());

  clients_.push_back(client);
  client->OnSessionStateChanged();
}

void RemotingSourceImpl::RemoveClient(Client* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = std::find(clients_.begin(), clients_.end(), client);
  DCHECK(it != clients_.end());

  clients_.erase(it);
  if (clients_.empty() && (state_ == RemotingSessionState::SESSION_STARTED ||
                           state_ == RemotingSessionState::SESSION_STARTING)) {
    remoter_->Stop(mojom::RemotingStopReason::SOURCE_GONE);
    state_ = RemotingSessionState::SESSION_STOPPING;
  }
}

void RemotingSourceImpl::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == RemotingSessionState::SESSION_STARTED ||
      state_ == RemotingSessionState::SESSION_STARTING)
    remoter_->Stop(mojom::RemotingStopReason::UNEXPECTED_FAILURE);
  UpdateAndNotifyState(RemotingSessionState::SESSION_PERMANENTLY_STOPPED);
}

void RemotingSourceImpl::StartDataPipe(
    std::unique_ptr<mojo::DataPipe> audio_data_pipe,
    std::unique_ptr<mojo::DataPipe> video_data_pipe,
    const DataPipeStartCallback& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!done_callback.is_null());

  bool audio = audio_data_pipe != nullptr;
  bool video = video_data_pipe != nullptr;
  if (!audio && !video) {
    LOG(ERROR) << "No audio and video to establish data pipe";
    done_callback.Run(mojom::RemotingDataStreamSenderPtrInfo(),
                      mojom::RemotingDataStreamSenderPtrInfo(),
                      mojo::ScopedDataPipeProducerHandle(),
                      mojo::ScopedDataPipeProducerHandle());
    return;
  }
  mojom::RemotingDataStreamSenderPtr audio_stream_sender;
  mojom::RemotingDataStreamSenderPtr video_stream_sender;
  remoter_->StartDataStreams(
      audio ? std::move(audio_data_pipe->consumer_handle)
            : mojo::ScopedDataPipeConsumerHandle(),
      video ? std::move(video_data_pipe->consumer_handle)
            : mojo::ScopedDataPipeConsumerHandle(),
      audio ? mojo::GetProxy(&audio_stream_sender)
            : media::mojom::RemotingDataStreamSenderRequest(),
      video ? mojo::GetProxy(&video_stream_sender)
            : media::mojom::RemotingDataStreamSenderRequest());
  done_callback.Run(audio_stream_sender.PassInterface(),
                    video_stream_sender.PassInterface(),
                    audio ? std::move(audio_data_pipe->producer_handle)
                          : mojo::ScopedDataPipeProducerHandle(),
                    video ? std::move(video_data_pipe->producer_handle)
                          : mojo::ScopedDataPipeProducerHandle());
}

remoting::RpcBroker* RemotingSourceImpl::GetRpcBroker() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(xjz): Fix the const-correctness.
  return const_cast<remoting::RpcBroker*>(&rpc_broker_);
}

void RemotingSourceImpl::SendMessageToSink(
    std::unique_ptr<std::vector<uint8_t>> message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  remoter_->SendMessageToSink(*message);
}

}  // namespace media
