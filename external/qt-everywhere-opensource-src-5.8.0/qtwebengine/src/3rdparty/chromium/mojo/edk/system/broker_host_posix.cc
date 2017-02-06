// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/broker_host.h"

#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/broker_messages.h"

namespace mojo {
namespace edk {

namespace {
// To prevent abuse, limit the maximum size of shared memory buffers.
// TODO(amistry): Re-consider this limit, or do something smarter.
const size_t kMaxSharedBufferSize = 16 * 1024 * 1024;
}

BrokerHost::BrokerHost(ScopedPlatformHandle platform_handle) {
  CHECK(platform_handle.is_valid());

  base::MessageLoop::current()->AddDestructionObserver(this);

  channel_ = Channel::Create(this, std::move(platform_handle),
                             base::ThreadTaskRunnerHandle::Get());
  channel_->Start();
}

BrokerHost::~BrokerHost() {
  // We're always destroyed on the creation thread, which is the IO thread.
  base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (channel_)
    channel_->ShutDown();
}

void BrokerHost::SendChannel(ScopedPlatformHandle handle) {
  CHECK(handle.is_valid());
  CHECK(channel_);

  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, nullptr);
  ScopedPlatformHandleVectorPtr handles;
  handles.reset(new PlatformHandleVector(1));
  handles->at(0) = handle.release();
  message->SetHandles(std::move(handles));

  channel_->Write(std::move(message));
}

void BrokerHost::OnBufferRequest(size_t num_bytes) {
  scoped_refptr<PlatformSharedBuffer> buffer;
  scoped_refptr<PlatformSharedBuffer> read_only_buffer;
  if (num_bytes <= kMaxSharedBufferSize) {
    buffer = PlatformSharedBuffer::Create(num_bytes);
    if (buffer)
      read_only_buffer = buffer->CreateReadOnlyDuplicate();
    if (!read_only_buffer)
      buffer = nullptr;
  } else {
    LOG(ERROR) << "Shared buffer request too large: " << num_bytes;
  }

  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_RESPONSE, buffer ? 2 : 0, nullptr);
  if (buffer) {
    ScopedPlatformHandleVectorPtr handles;
    handles.reset(new PlatformHandleVector(2));
    handles->at(0) = buffer->PassPlatformHandle().release();
    handles->at(1) = read_only_buffer->PassPlatformHandle().release();
    message->SetHandles(std::move(handles));
  }

  channel_->Write(std::move(message));
}

void BrokerHost::OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  ScopedPlatformHandleVectorPtr handles) {
  if (payload_size < sizeof(BrokerMessageHeader))
    return;

  const BrokerMessageHeader* header =
      static_cast<const BrokerMessageHeader*>(payload);
  switch (header->type) {
    case BrokerMessageType::BUFFER_REQUEST:
      if (payload_size ==
            sizeof(BrokerMessageHeader) + sizeof(BufferRequestData)) {
        const BufferRequestData* request =
            reinterpret_cast<const BufferRequestData*>(header + 1);
        OnBufferRequest(request->size);
        return;
      }
      break;

    default:
      break;
  }

  LOG(ERROR) << "Unexpected broker message type: " << header->type;
}

void BrokerHost::OnChannelError() {
  if (channel_) {
    channel_->ShutDown();
    channel_ = nullptr;
  }

  delete this;
}

void BrokerHost::WillDestroyCurrentMessageLoop() {
  delete this;
}

}  // namespace edk
}  // namespace mojo
