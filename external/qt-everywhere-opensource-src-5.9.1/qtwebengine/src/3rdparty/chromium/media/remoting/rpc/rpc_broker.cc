// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/rpc/rpc_broker.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"

namespace media {
namespace remoting {

namespace {
// Generates unique handle value.
base::StaticAtomicSequenceNumber g_handle_generator;
}

RpcBroker::RpcBroker(const SendMessageCallback& send_message_cb)
    : send_message_cb_(send_message_cb), weak_factory_(this) {}

RpcBroker::~RpcBroker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  receive_callbacks_.clear();
}

// static
int RpcBroker::GetUniqueHandle() {
  // Return the next handle. If GetNext() returns zero (kReceiverHandle), call
  // it again so that 1+ are returned by this function.
  int handle = g_handle_generator.GetNext();
  if (handle != kReceiverHandle)
    return handle;
  return g_handle_generator.GetNext();
}

void RpcBroker::RegisterMessageReceiverCallback(
    int handle,
    const ReceiveMessageCallback& callback) {
  VLOG(2) << __FUNCTION__ << "handle=" << handle;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(receive_callbacks_.find(handle) == receive_callbacks_.end());
  receive_callbacks_[handle] = callback;
}

void RpcBroker::UnregisterMessageReceiverCallback(int handle) {
  VLOG(2) << __FUNCTION__ << " handle=" << handle;
  DCHECK(thread_checker_.CalledOnValidThread());
  receive_callbacks_.erase(handle);
}

void RpcBroker::ProcessMessageFromRemote(
    std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(message);
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto entry = receive_callbacks_.find(message->handle());
  if (entry == receive_callbacks_.end()) {
    LOG(ERROR) << "unregistered handle: " << message->handle();
    return;
  }
  entry->second.Run(std::move(message));
}

void RpcBroker::SendMessageToRemote(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message);
  std::unique_ptr<std::vector<uint8_t>> serialized_message(
      new std::vector<uint8_t>(message->ByteSize()));
  CHECK(message->SerializeToArray(serialized_message->data(),
                                  serialized_message->size()));
  send_message_cb_.Run(std::move(serialized_message));
}

base::WeakPtr<RpcBroker> RpcBroker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RpcBroker::SetMessageCallbackForTesting(
    const SendMessageCallback& send_message_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  send_message_cb_ = send_message_cb;
}

}  // namespace remoting
}  // namespace media
