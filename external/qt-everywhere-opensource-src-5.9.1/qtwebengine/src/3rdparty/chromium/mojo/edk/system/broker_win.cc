// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <limits>
#include <utility>

#include "base/debug/alias.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/broker_messages.h"
#include "mojo/edk/system/channel.h"

namespace mojo {
namespace edk {

namespace {

// 256 bytes should be enough for anyone!
const size_t kMaxBrokerMessageSize = 256;

bool TakeHandlesFromBrokerMessage(Channel::Message* message,
                                  size_t num_handles,
                                  ScopedPlatformHandle* out_handles) {
  if (message->num_handles() != num_handles) {
    DLOG(ERROR) << "Received unexpected number of handles in broker message";
    return false;
  }

  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  DCHECK(handles);
  DCHECK_EQ(handles->size(), num_handles);
  DCHECK(out_handles);

  for (size_t i = 0; i < num_handles; ++i)
    out_handles[i] = ScopedPlatformHandle((*handles)[i]);
  handles->clear();
  return true;
}

Channel::MessagePtr WaitForBrokerMessage(PlatformHandle platform_handle,
                                         BrokerMessageType expected_type) {
  char buffer[kMaxBrokerMessageSize];
  DWORD bytes_read = 0;
  BOOL result = ::ReadFile(platform_handle.handle, buffer,
                           kMaxBrokerMessageSize, &bytes_read, nullptr);
  if (!result) {
    // The pipe may be broken if the browser side has been closed, e.g. during
    // browser shutdown. In that case the ReadFile call will fail and we
    // shouldn't continue waiting.
    PLOG(ERROR) << "Error reading broker pipe";
    return nullptr;
  }

  Channel::MessagePtr message =
      Channel::Message::Deserialize(buffer, static_cast<size_t>(bytes_read));
  if (!message || message->payload_size() < sizeof(BrokerMessageHeader)) {
    LOG(ERROR) << "Invalid broker message";

    base::debug::Alias(&buffer[0]);
    base::debug::Alias(&bytes_read);
    base::debug::Alias(message.get());
    CHECK(false);
    return nullptr;
  }

  const BrokerMessageHeader* header =
      reinterpret_cast<const BrokerMessageHeader*>(message->payload());
  if (header->type != expected_type) {
    LOG(ERROR) << "Unexpected broker message type";

    base::debug::Alias(&buffer[0]);
    base::debug::Alias(&bytes_read);
    base::debug::Alias(message.get());
    CHECK(false);
    return nullptr;
  }

  return message;
}

}  // namespace

Broker::Broker(ScopedPlatformHandle handle) : sync_channel_(std::move(handle)) {
  CHECK(sync_channel_.is_valid());
  Channel::MessagePtr message =
      WaitForBrokerMessage(sync_channel_.get(), BrokerMessageType::INIT);

  // If we fail to read a message (broken pipe), just return early. The parent
  // handle will be null and callers must handle this gracefully.
  if (!message)
    return;

  if (!TakeHandlesFromBrokerMessage(message.get(), 1, &parent_channel_)) {
    // If the message has no handles, we expect it to carry pipe name instead.
    const BrokerMessageHeader* header =
        static_cast<const BrokerMessageHeader*>(message->payload());
    CHECK_GE(message->payload_size(),
             sizeof(BrokerMessageHeader) + sizeof(InitData));
    const InitData* data = reinterpret_cast<const InitData*>(header + 1);
    CHECK_EQ(message->payload_size(),
             sizeof(BrokerMessageHeader) + sizeof(InitData) +
             data->pipe_name_length * sizeof(base::char16));
    const base::char16* name_data =
        reinterpret_cast<const base::char16*>(data + 1);
    CHECK(data->pipe_name_length);
    parent_channel_ = CreateClientHandle(NamedPlatformHandle(
        base::StringPiece16(name_data, data->pipe_name_length)));
  }
}

Broker::~Broker() {}

ScopedPlatformHandle Broker::GetParentPlatformHandle() {
  return std::move(parent_channel_);
}

scoped_refptr<PlatformSharedBuffer> Broker::GetSharedBuffer(size_t num_bytes) {
  base::AutoLock lock(lock_);
  BufferRequestData* buffer_request;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_REQUEST, 0, 0, &buffer_request);
  buffer_request->size = base::checked_cast<uint32_t>(num_bytes);
  DWORD bytes_written = 0;
  BOOL result = ::WriteFile(sync_channel_.get().handle, out_message->data(),
                            static_cast<DWORD>(out_message->data_num_bytes()),
                            &bytes_written, nullptr);
  if (!result ||
      static_cast<size_t>(bytes_written) != out_message->data_num_bytes()) {
    LOG(ERROR) << "Error sending sync broker message";
    return nullptr;
  }

  ScopedPlatformHandle handles[2];
  Channel::MessagePtr response = WaitForBrokerMessage(
      sync_channel_.get(), BrokerMessageType::BUFFER_RESPONSE);
  if (response &&
      TakeHandlesFromBrokerMessage(response.get(), 2, &handles[0])) {
    return PlatformSharedBuffer::CreateFromPlatformHandlePair(
        num_bytes, std::move(handles[0]), std::move(handles[1]));
  }

  return nullptr;
}

}  // namespace edk
}  // namespace mojo
