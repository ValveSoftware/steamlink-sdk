// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some basic types used by the P2P-related IPC
// messages.

#ifndef CONTENT_COMMON_P2P_SOCKET_TYPE_H_
#define CONTENT_COMMON_P2P_SOCKET_TYPE_H_

#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "net/base/ip_endpoint.h"

namespace content {

enum P2PSocketOption {
  P2P_SOCKET_OPT_RCVBUF,  // Receive buffer size.
  P2P_SOCKET_OPT_SNDBUF,  // Send buffer size.
  P2P_SOCKET_OPT_DSCP,    // DSCP code.
  P2P_SOCKET_OPT_MAX
};

// Type of P2P Socket.
enum P2PSocketType {
  P2P_SOCKET_UDP,
  P2P_SOCKET_TCP_SERVER,
  P2P_SOCKET_STUN_TCP_SERVER,
  P2P_SOCKET_TCP_CLIENT,
  P2P_SOCKET_STUN_TCP_CLIENT,
  P2P_SOCKET_SSLTCP_CLIENT,
  P2P_SOCKET_STUN_SSLTCP_CLIENT,
  P2P_SOCKET_TLS_CLIENT,
  P2P_SOCKET_STUN_TLS_CLIENT,
  P2P_SOCKET_TYPE_LAST = P2P_SOCKET_STUN_TLS_CLIENT
};

// Struct which carries both resolved IP address and host string literal.
// Port number will be part of |ip_address|.
struct P2PHostAndIPEndPoint {
  P2PHostAndIPEndPoint() {}
  P2PHostAndIPEndPoint(const std::string& hostname,
                       const net::IPEndPoint& ip_address)
      : hostname(hostname), ip_address(ip_address) {
  }

  std::string hostname;
  net::IPEndPoint ip_address;
};

// Struct which keeps track of metrics during a send operation on P2P sockets.
struct P2PSendPacketMetrics {
  P2PSendPacketMetrics() {}
  P2PSendPacketMetrics(uint64_t packet_id,
                       int32_t rtc_packet_id,
                       base::TimeTicks send_time)
      : packet_id(packet_id),
        rtc_packet_id(rtc_packet_id),
        send_time(send_time) {}

  uint64_t packet_id = 0;
  // rtc_packet_id is a sequential packet counter written in the RTP header and
  // used by RTP receivers to ACK received packets. It is sent back with a
  // corresponding send time to WebRTC in the browser process so that it can be
  // combined with ACKs to compute inter-packet delay variations.
  int32_t rtc_packet_id = -1;
  base::TimeTicks send_time;
};

}  // namespace content

#endif  // CONTENT_COMMON_P2P_SOCKET_TYPE_H_
