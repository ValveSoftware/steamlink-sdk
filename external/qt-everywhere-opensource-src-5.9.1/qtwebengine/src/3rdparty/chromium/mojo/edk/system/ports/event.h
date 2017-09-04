// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_EVENT_H_
#define MOJO_EDK_SYSTEM_PORTS_EVENT_H_

#include <stdint.h>

#include "mojo/edk/system/ports/message.h"
#include "mojo/edk/system/ports/name.h"

namespace mojo {
namespace edk {
namespace ports {

#pragma pack(push, 1)

// TODO: Add static assertions of alignment.

struct PortDescriptor {
  PortDescriptor();

  NodeName peer_node_name;
  PortName peer_port_name;
  NodeName referring_node_name;
  PortName referring_port_name;
  uint64_t next_sequence_num_to_send;
  uint64_t next_sequence_num_to_receive;
  uint64_t last_sequence_num_to_receive;
  bool peer_closed;
  char padding[7];
};

enum struct EventType : uint32_t {
  kUser,
  kPortAccepted,
  kObserveProxy,
  kObserveProxyAck,
  kObserveClosure,
  kMergePort,
};

struct EventHeader {
  EventType type;
  uint32_t padding;
  PortName port_name;
};

struct UserEventData {
  uint64_t sequence_num;
  uint32_t num_ports;
  uint32_t padding;
};

struct ObserveProxyEventData {
  NodeName proxy_node_name;
  PortName proxy_port_name;
  NodeName proxy_to_node_name;
  PortName proxy_to_port_name;
};

struct ObserveProxyAckEventData {
  uint64_t last_sequence_num;
};

struct ObserveClosureEventData {
  uint64_t last_sequence_num;
};

struct MergePortEventData {
  PortName new_port_name;
  PortDescriptor new_port_descriptor;
};

#pragma pack(pop)

inline const EventHeader* GetEventHeader(const Message& message) {
  return static_cast<const EventHeader*>(message.header_bytes());
}

inline EventHeader* GetMutableEventHeader(Message* message) {
  return static_cast<EventHeader*>(message->mutable_header_bytes());
}

template <typename EventData>
inline const EventData* GetEventData(const Message& message) {
  return reinterpret_cast<const EventData*>(
      reinterpret_cast<const char*>(GetEventHeader(message) + 1));
}

template <typename EventData>
inline EventData* GetMutableEventData(Message* message) {
  return reinterpret_cast<EventData*>(
      reinterpret_cast<char*>(GetMutableEventHeader(message) + 1));
}

inline const PortDescriptor* GetPortDescriptors(const UserEventData* event) {
  return reinterpret_cast<const PortDescriptor*>(
      reinterpret_cast<const char*>(event + 1));
}

inline PortDescriptor* GetMutablePortDescriptors(UserEventData* event) {
  return reinterpret_cast<PortDescriptor*>(reinterpret_cast<char*>(event + 1));
}

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_EVENT_H_
