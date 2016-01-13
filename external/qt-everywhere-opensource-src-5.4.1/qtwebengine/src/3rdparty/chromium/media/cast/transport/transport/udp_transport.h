// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_TRANSPORT_UDP_TRANSPORT_H_
#define MEDIA_CAST_TRANSPORT_TRANSPORT_UDP_TRANSPORT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_environment.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/udp/udp_socket.h"

namespace net {
class IOBuffer;
class IPEndPoint;
class NetLog;
}  // namespace net

namespace media {
namespace cast {
namespace transport {

// This class implements UDP transport mechanism for Cast.
class UdpTransport : public PacketSender {
 public:
  // Construct a UDP transport.
  // All methods must be called on |io_thread_proxy|.
  // |local_end_point| specifies the address and port to bind and listen
  // to incoming packets. If the value is 0.0.0.0:0 then a bind is not
  // performed.
  // |remote_end_point| specifies the address and port to send packets
  // to. If the value is 0.0.0.0:0 the the end point is set to the source
  // address of the first packet received.
  UdpTransport(
      net::NetLog* net_log,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_proxy,
      const net::IPEndPoint& local_end_point,
      const net::IPEndPoint& remote_end_point,
      const CastTransportStatusCallback& status_callback);
  virtual ~UdpTransport();

  // Start receiving packets. Packets are submitted to |packet_receiver|.
  void StartReceiving(const PacketReceiverCallback& packet_receiver);

  // Set a new DSCP value to the socket. The value will be set right before
  // the next send.
  void SetDscp(net::DiffServCodePoint dscp);

  // PacketSender implementations.
  virtual bool SendPacket(PacketRef packet,
                          const base::Closure& cb) OVERRIDE;

 private:
  // Requests and processes packets from |udp_socket_|.  This method is called
  // once with |length_or_status| set to net::ERR_IO_PENDING to start receiving
  // packets.  Thereafter, it is called with some other value as the callback
  // response from UdpSocket::RecvFrom().
  void ReceiveNextPacket(int length_or_status);

  // Schedule packet receiving, if needed.
  void ScheduleReceiveNextPacket();

  void OnSent(const scoped_refptr<net::IOBuffer>& buf,
              PacketRef packet,
              const base::Closure& cb,
              int result);

  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy_;
  const net::IPEndPoint local_addr_;
  net::IPEndPoint remote_addr_;
  const scoped_ptr<net::UDPSocket> udp_socket_;
  bool send_pending_;
  bool receive_pending_;
  bool client_connected_;
  net::DiffServCodePoint next_dscp_value_;
  scoped_ptr<Packet> next_packet_;
  scoped_refptr<net::WrappedIOBuffer> recv_buf_;
  net::IPEndPoint recv_addr_;
  PacketReceiverCallback packet_receiver_;
  const CastTransportStatusCallback status_callback_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<UdpTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UdpTransport);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_UDP_TRANSPORT_H_
