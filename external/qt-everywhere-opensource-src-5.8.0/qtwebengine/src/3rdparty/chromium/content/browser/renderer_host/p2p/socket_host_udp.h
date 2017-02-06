// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/content_export.h"
#include "content/common/p2p_socket_type.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/diff_serv_code_point.h"
#include "net/udp/udp_server_socket.h"
#include "third_party/webrtc/base/asyncpacketsocket.h"

namespace content {

class P2PMessageThrottler;

class CONTENT_EXPORT P2PSocketHostUdp : public P2PSocketHost {
 public:
  P2PSocketHostUdp(IPC::Sender* message_sender,
                   int socket_id,
                   P2PMessageThrottler* throttler);
  ~P2PSocketHostUdp() override;

  // P2PSocketHost overrides.
  bool Init(const net::IPEndPoint& local_address,
            const P2PHostAndIPEndPoint& remote_address) override;
  void Send(const net::IPEndPoint& to,
            const std::vector<char>& data,
            const rtc::PacketOptions& options,
            uint64_t packet_id) override;
  P2PSocketHost* AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address,
      int id) override;
  bool SetOption(P2PSocketOption option, int value) override;

 private:
  friend class P2PSocketHostUdpTest;

  typedef std::set<net::IPEndPoint> ConnectedPeerSet;

  struct PendingPacket {
    PendingPacket(const net::IPEndPoint& to,
                  const std::vector<char>& content,
                  const rtc::PacketOptions& options,
                  uint64_t id);
    PendingPacket(const PendingPacket& other);
    ~PendingPacket();
    net::IPEndPoint to;
    scoped_refptr<net::IOBuffer> data;
    int size;
    rtc::PacketOptions packet_options;
    uint64_t id;
  };

  void OnError();

  void SetSendBufferSize();

  void DoRead();
  void OnRecv(int result);
  void HandleReadResult(int result);

  void DoSend(const PendingPacket& packet);
  void OnSend(uint64_t packet_id,
              int32_t transport_sequence_number,
              base::TimeTicks send_time,
              int result);
  void HandleSendResult(uint64_t packet_id,
                        int32_t transport_sequence_number,
                        base::TimeTicks send_time,
                        int result);

  std::unique_ptr<net::DatagramServerSocket> socket_;
  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint recv_address_;

  std::deque<PendingPacket> send_queue_;
  bool send_pending_;
  net::DiffServCodePoint last_dscp_;

  // Set of peer for which we have received STUN binding request or
  // response or relay allocation request or response.
  ConnectedPeerSet connected_peers_;
  P2PMessageThrottler* throttler_;

  // Keep track of the send socket buffer size under experiment.
  size_t send_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHostUdp);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_UDP_H_
