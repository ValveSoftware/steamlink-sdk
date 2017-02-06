// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/broker.h"

#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_utils.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/broker_messages.h"
#include "mojo/edk/system/channel.h"

namespace mojo {
namespace edk {

namespace {

bool WaitForBrokerMessage(PlatformHandle platform_handle,
                          BrokerMessageType expected_type,
                          size_t expected_num_handles,
                          std::deque<PlatformHandle>* incoming_handles) {
  Channel::MessagePtr message(
      new Channel::Message(sizeof(BrokerMessageHeader), expected_num_handles));
  std::deque<PlatformHandle> incoming_platform_handles;
  ssize_t read_result = PlatformChannelRecvmsg(
      platform_handle, const_cast<void*>(message->data()),
      message->data_num_bytes(), &incoming_platform_handles, true /* block */);
  bool error = false;
  if (read_result < 0) {
    PLOG(ERROR) << "Recvmsg error";
    error = true;
  } else if (static_cast<size_t>(read_result) != message->data_num_bytes()) {
    LOG(ERROR) << "Invalid node channel message";
    error = true;
  } else if (incoming_platform_handles.size() != expected_num_handles) {
    LOG(ERROR) << "Received unexpected number of handles";
    error = true;
  }

  if (!error) {
    const BrokerMessageHeader* header =
        reinterpret_cast<const BrokerMessageHeader*>(message->payload());
    if (header->type != expected_type) {
      LOG(ERROR) << "Unexpected message";
      error = true;
    }
  }

  if (error) {
    CloseAllPlatformHandles(&incoming_platform_handles);
  } else {
    if (incoming_handles)
      incoming_handles->swap(incoming_platform_handles);
  }
  return !error;
}

}  // namespace

Broker::Broker(ScopedPlatformHandle platform_handle)
    : sync_channel_(std::move(platform_handle)) {
  CHECK(sync_channel_.is_valid());

  // Mark the channel as blocking.
  int flags = fcntl(sync_channel_.get().handle, F_GETFL);
  PCHECK(flags != -1);
  flags = fcntl(sync_channel_.get().handle, F_SETFL, flags & ~O_NONBLOCK);
  PCHECK(flags != -1);

  // Wait for the first message, which should contain a handle.
  std::deque<PlatformHandle> incoming_platform_handles;
  if (WaitForBrokerMessage(sync_channel_.get(), BrokerMessageType::INIT, 1,
                           &incoming_platform_handles)) {
    parent_channel_ = ScopedPlatformHandle(incoming_platform_handles.front());
  }
}

Broker::~Broker() = default;

ScopedPlatformHandle Broker::GetParentPlatformHandle() {
  return std::move(parent_channel_);
}

scoped_refptr<PlatformSharedBuffer> Broker::GetSharedBuffer(size_t num_bytes) {
  base::AutoLock lock(lock_);

  BufferRequestData* buffer_request;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_REQUEST, 0, &buffer_request);
  buffer_request->size = num_bytes;
  ssize_t write_result = PlatformChannelWrite(
      sync_channel_.get(), out_message->data(), out_message->data_num_bytes());
  if (write_result < 0) {
    PLOG(ERROR) << "Error sending sync broker message";
    return nullptr;
  } else if (static_cast<size_t>(write_result) !=
             out_message->data_num_bytes()) {
    LOG(ERROR) << "Error sending complete broker message";
    return nullptr;
  }

  std::deque<PlatformHandle> incoming_platform_handles;
  if (WaitForBrokerMessage(sync_channel_.get(),
                           BrokerMessageType::BUFFER_RESPONSE, 2,
                           &incoming_platform_handles)) {
    ScopedPlatformHandle rw_handle(incoming_platform_handles.front());
    incoming_platform_handles.pop_front();
    ScopedPlatformHandle ro_handle(incoming_platform_handles.front());
    return PlatformSharedBuffer::CreateFromPlatformHandlePair(
        num_bytes, std::move(rw_handle), std::move(ro_handle));
  }

  return nullptr;
}

}  // namespace edk
}  // namespace mojo
