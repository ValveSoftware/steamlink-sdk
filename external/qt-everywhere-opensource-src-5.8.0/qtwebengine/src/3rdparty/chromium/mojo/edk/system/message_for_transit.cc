// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_for_transit.h"

#include <vector>

#include "mojo/edk/embedder/platform_handle_vector.h"

namespace mojo {
namespace edk {

namespace {

static_assert(sizeof(MessageForTransit::MessageHeader) % 8 == 0,
              "Invalid MessageHeader size.");
static_assert(sizeof(MessageForTransit::DispatcherHeader) % 8 == 0,
              "Invalid DispatcherHeader size.");

}  // namespace

MessageForTransit::~MessageForTransit() {}

// static
MojoResult MessageForTransit::Create(
    std::unique_ptr<MessageForTransit>* message,
    uint32_t num_bytes,
    const Dispatcher::DispatcherInTransit* dispatchers,
    uint32_t num_dispatchers) {
  // A structure for retaining information about every Dispatcher that will be
  // sent with this message.
  struct DispatcherInfo {
    uint32_t num_bytes;
    uint32_t num_ports;
    uint32_t num_handles;
  };

  // This is only the base header size. It will grow as we accumulate the
  // size of serialized state for each dispatcher.
  size_t header_size = sizeof(MessageHeader) +
      num_dispatchers * sizeof(DispatcherHeader);
  size_t num_ports = 0;
  size_t num_handles = 0;

  std::vector<DispatcherInfo> dispatcher_info(num_dispatchers);
  for (size_t i = 0; i < num_dispatchers; ++i) {
    Dispatcher* d = dispatchers[i].dispatcher.get();
    d->StartSerialize(&dispatcher_info[i].num_bytes,
                      &dispatcher_info[i].num_ports,
                      &dispatcher_info[i].num_handles);
    header_size += dispatcher_info[i].num_bytes;
    num_ports += dispatcher_info[i].num_ports;
    num_handles += dispatcher_info[i].num_handles;
  }

  // We now have enough information to fully allocate the message storage.
  std::unique_ptr<PortsMessage> msg = PortsMessage::NewUserMessage(
      header_size + num_bytes, num_ports, num_handles);
  if (!msg)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  // Populate the message header with information about serialized dispatchers.
  //
  // The front of the message is always a MessageHeader followed by a
  // DispatcherHeader for each dispatcher to be sent.
  MessageHeader* header =
      static_cast<MessageHeader*>(msg->mutable_payload_bytes());
  DispatcherHeader* dispatcher_headers =
      reinterpret_cast<DispatcherHeader*>(header + 1);

  // Serialized dispatcher state immediately follows the series of
  // DispatcherHeaders.
  char* dispatcher_data =
      reinterpret_cast<char*>(dispatcher_headers + num_dispatchers);

  header->num_dispatchers = num_dispatchers;

  // |header_size| is the total number of bytes preceding the message payload,
  // including all dispatcher headers and serialized dispatcher state.
  DCHECK_LE(header_size, std::numeric_limits<uint32_t>::max());
  header->header_size = static_cast<uint32_t>(header_size);

  if (num_dispatchers > 0) {
    ScopedPlatformHandleVectorPtr handles(
        new PlatformHandleVector(num_handles));
    size_t port_index = 0;
    size_t handle_index = 0;
    bool fail = false;
    for (size_t i = 0; i < num_dispatchers; ++i) {
      Dispatcher* d = dispatchers[i].dispatcher.get();
      DispatcherHeader* dh = &dispatcher_headers[i];
      const DispatcherInfo& info = dispatcher_info[i];

      // Fill in the header for this dispatcher.
      dh->type = static_cast<int32_t>(d->GetType());
      dh->num_bytes = info.num_bytes;
      dh->num_ports = info.num_ports;
      dh->num_platform_handles = info.num_handles;

      // Fill in serialized state, ports, and platform handles. We'll cancel
      // the send if the dispatcher implementation rejects for some reason.
      if (!d->EndSerialize(static_cast<void*>(dispatcher_data),
                           msg->mutable_ports() + port_index,
                           handles->data() + handle_index)) {
        fail = true;
        break;
      }

      dispatcher_data += info.num_bytes;
      port_index += info.num_ports;
      handle_index += info.num_handles;
    }

    if (fail) {
      // Release any platform handles we've accumulated. Their dispatchers
      // retain ownership when message creation fails, so these are not actually
      // leaking.
      handles->clear();
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    // Take ownership of all the handles and move them into message storage.
    msg->SetHandles(std::move(handles));
  }

  message->reset(new MessageForTransit(std::move(msg)));
  return MOJO_RESULT_OK;
}

MessageForTransit::MessageForTransit(std::unique_ptr<PortsMessage> message)
    : message_(std::move(message)) {
}

}  // namespace edk
}  // namespace mojo
