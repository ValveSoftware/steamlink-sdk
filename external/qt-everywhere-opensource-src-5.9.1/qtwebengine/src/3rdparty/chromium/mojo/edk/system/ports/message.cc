// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <limits>

#include "base/logging.h"
#include "mojo/edk/system/ports/event.h"

namespace mojo {
namespace edk {
namespace ports {

// static
bool Message::Parse(const void* bytes,
                    size_t num_bytes,
                    size_t* num_header_bytes,
                    size_t* num_payload_bytes,
                    size_t* num_ports_bytes) {
  if (num_bytes < sizeof(EventHeader))
    return false;
  const EventHeader* header = static_cast<const EventHeader*>(bytes);
  switch (header->type) {
    case EventType::kUser:
      // See below.
      break;
    case EventType::kPortAccepted:
      *num_header_bytes = sizeof(EventHeader);
      break;
    case EventType::kObserveProxy:
      *num_header_bytes = sizeof(EventHeader) + sizeof(ObserveProxyEventData);
      break;
    case EventType::kObserveProxyAck:
      *num_header_bytes =
          sizeof(EventHeader) + sizeof(ObserveProxyAckEventData);
      break;
    case EventType::kObserveClosure:
      *num_header_bytes = sizeof(EventHeader) + sizeof(ObserveClosureEventData);
      break;
    case EventType::kMergePort:
      *num_header_bytes = sizeof(EventHeader) + sizeof(MergePortEventData);
      break;
    default:
      return false;
  }

  if (header->type == EventType::kUser) {
    if (num_bytes < sizeof(EventHeader) + sizeof(UserEventData))
      return false;
    const UserEventData* event_data =
        reinterpret_cast<const UserEventData*>(
            reinterpret_cast<const char*>(header + 1));
    if (event_data->num_ports > std::numeric_limits<uint16_t>::max())
      return false;
    *num_header_bytes = sizeof(EventHeader) +
                        sizeof(UserEventData) +
                        event_data->num_ports * sizeof(PortDescriptor);
    *num_ports_bytes = event_data->num_ports * sizeof(PortName);
    if (num_bytes < *num_header_bytes + *num_ports_bytes)
      return false;
    *num_payload_bytes = num_bytes - *num_header_bytes - *num_ports_bytes;
  } else {
    if (*num_header_bytes != num_bytes)
      return false;
    *num_payload_bytes = 0;
    *num_ports_bytes = 0;
  }

  return true;
}

Message::Message(size_t num_payload_bytes, size_t num_ports)
    : Message(sizeof(EventHeader) + sizeof(UserEventData) +
                  num_ports * sizeof(PortDescriptor),
              num_payload_bytes, num_ports * sizeof(PortName)) {
  num_ports_ = num_ports;
}

Message::Message(size_t num_header_bytes,
                 size_t num_payload_bytes,
                 size_t num_ports_bytes)
    : start_(nullptr),
      num_header_bytes_(num_header_bytes),
      num_ports_bytes_(num_ports_bytes),
      num_payload_bytes_(num_payload_bytes) {
}

void Message::InitializeUserMessageHeader(void* start) {
  start_ = static_cast<char*>(start);
  memset(start_, 0, num_header_bytes_);
  GetMutableEventHeader(this)->type = EventType::kUser;
  GetMutableEventData<UserEventData>(this)->num_ports =
      static_cast<uint32_t>(num_ports_);
}

}  // namespace ports
}  // namespace edk
}  // namespace mojo
