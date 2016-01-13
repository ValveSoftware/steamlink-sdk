// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport/udp_transport.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/cast/test/utility/net_utility.h"
#include "media/cast/transport/cast_transport_config.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace transport {

class MockPacketReceiver {
 public:
  MockPacketReceiver(const base::Closure& callback)
      : packet_callback_(callback) {}

  void ReceivedPacket(scoped_ptr<Packet> packet) {
    packet_ = std::string(packet->size(), '\0');
    std::copy(packet->begin(), packet->end(), packet_.begin());
    packet_callback_.Run();
  }

  std::string packet() const { return packet_; }
  transport::PacketReceiverCallback packet_receiver() {
    return base::Bind(&MockPacketReceiver::ReceivedPacket,
                      base::Unretained(this));
  }

 private:
  std::string packet_;
  base::Closure packet_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockPacketReceiver);
};

void SendPacket(UdpTransport* transport, Packet packet) {
  base::Closure cb;
  transport->SendPacket(new base::RefCountedData<Packet>(packet), cb);
}

static void UpdateCastTransportStatus(transport::CastTransportStatus status) {
  NOTREACHED();
}

TEST(UdpTransport, SendAndReceive) {
  base::MessageLoopForIO message_loop;

  net::IPEndPoint free_local_port1 = test::GetFreeLocalPort();
  net::IPEndPoint free_local_port2 = test::GetFreeLocalPort();
  net::IPAddressNumber empty_addr_number;
  net::ParseIPLiteralToNumber("0.0.0.0", &empty_addr_number);

  UdpTransport send_transport(NULL,
                              message_loop.message_loop_proxy(),
                              free_local_port1,
                              free_local_port2,
                              base::Bind(&UpdateCastTransportStatus));
  UdpTransport recv_transport(NULL,
                              message_loop.message_loop_proxy(),
                              free_local_port2,
                              net::IPEndPoint(empty_addr_number, 0),
                              base::Bind(&UpdateCastTransportStatus));

  Packet packet;
  packet.push_back('t');
  packet.push_back('e');
  packet.push_back('s');
  packet.push_back('t');

  base::RunLoop run_loop;
  MockPacketReceiver receiver1(run_loop.QuitClosure());
  MockPacketReceiver receiver2(
      base::Bind(&SendPacket, &recv_transport, packet));
  send_transport.StartReceiving(receiver1.packet_receiver());
  recv_transport.StartReceiving(receiver2.packet_receiver());

  base::Closure cb;
  send_transport.SendPacket(new base::RefCountedData<Packet>(packet), cb);
  run_loop.Run();
  EXPECT_TRUE(
      std::equal(packet.begin(), packet.end(), receiver1.packet().begin()));
  EXPECT_TRUE(
      std::equal(packet.begin(), packet.end(), receiver2.packet().begin()));
}

}  // namespace transport
}  // namespace cast
}  // namespace media
