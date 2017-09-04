// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_tcp.h"

#include <stddef.h>
#include <stdint.h>

#include <deque>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sys_byteorder.h"
#include "content/browser/renderer_host/p2p/socket_host_test_utils.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;

namespace content {

class P2PSocketHostTcpTestBase : public testing::Test {
 protected:
  explicit P2PSocketHostTcpTestBase(P2PSocketType type)
      : socket_type_(type) {
  }

  void SetUp() override {
    EXPECT_CALL(
        sender_,
        Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSocketCreated::ID))))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

    if (socket_type_ == P2P_SOCKET_TCP_CLIENT) {
      socket_host_.reset(
          new P2PSocketHostTcp(&sender_, 0, P2P_SOCKET_TCP_CLIENT, nullptr));
    } else {
      socket_host_.reset(new P2PSocketHostStunTcp(
          &sender_, 0, P2P_SOCKET_STUN_TCP_CLIENT, nullptr));
    }

    socket_ = new FakeSocket(&sent_data_);
    socket_->SetLocalAddress(ParseAddress(kTestLocalIpAddress, kTestPort1));
    socket_host_->socket_.reset(socket_);

    dest_.ip_address = ParseAddress(kTestIpAddress1, kTestPort1);

    local_address_ = ParseAddress(kTestLocalIpAddress, kTestPort1);

    socket_host_->remote_address_ = dest_;
    socket_host_->state_ = P2PSocketHost::STATE_CONNECTING;
    socket_host_->OnConnected(net::OK);
  }

  std::string IntToSize(int size) {
    std::string result;
    uint16_t size16 = base::HostToNet16(size);
    result.resize(sizeof(size16));
    memcpy(&result[0], &size16, sizeof(size16));
    return result;
  }

  std::string sent_data_;
  FakeSocket* socket_;  // Owned by |socket_host_|.
  std::unique_ptr<P2PSocketHostTcpBase> socket_host_;
  MockIPCSender sender_;

  net::IPEndPoint local_address_;
  P2PHostAndIPEndPoint dest_;
  P2PSocketType socket_type_;
};

class P2PSocketHostTcpTest : public P2PSocketHostTcpTestBase {
 protected:
  P2PSocketHostTcpTest() : P2PSocketHostTcpTestBase(P2P_SOCKET_TCP_CLIENT) { }
};

class P2PSocketHostStunTcpTest : public P2PSocketHostTcpTestBase {
 protected:
  P2PSocketHostStunTcpTest()
      : P2PSocketHostTcpTestBase(P2P_SOCKET_STUN_TCP_CLIENT) {
  }
};

// Verify that we can send STUN message and that they are formatted
// properly.
TEST_F(P2PSocketHostTcpTest, SendStunNoAuth) {
  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .Times(3)
      .WillRepeatedly(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_.ip_address, packet1, options, 0);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_.ip_address, packet2, options, 0);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest_.ip_address, packet3, options, 0);

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(IntToSize(packet2.size()));
  expected_data.append(packet2.begin(), packet2.end());
  expected_data.append(IntToSize(packet3.size()));
  expected_data.append(packet3.begin(), packet3.end());

  EXPECT_EQ(expected_data, sent_data_);
}

// Verify that we can receive STUN messages from the socket, and that
// the messages are parsed properly.
TEST_F(P2PSocketHostTcpTest, ReceiveStun) {
  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .Times(3)
      .WillRepeatedly(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_.ip_address, packet1, options, 0);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_.ip_address, packet2, options, 0);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest_.ip_address, packet3, options, 0);

  std::string received_data;
  received_data.append(IntToSize(packet1.size()));
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(IntToSize(packet2.size()));
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(IntToSize(packet3.size()));
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet1)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet2)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet3)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= arraysize(step_sizes))
      step = 0;
  }
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketHostTcpTest, SendDataNoAuth) {
  EXPECT_CALL(sender_,
              Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnError::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest_.ip_address, packet, options, 0);

  EXPECT_EQ(0U, sent_data_.size());
}

// Verify that we can send data after we've received STUN response
// from the other side.
TEST_F(P2PSocketHostTcpTest, SendAfterStunRequest) {
  // Receive packet from |dest_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(request_packet)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_->AppendInputData(&received_data[0], received_data.size());

  rtc::PacketOptions options;
  // Now we should be able to send any data to |dest_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest_.ip_address, packet, options, 0);

  std::string expected_data;
  expected_data.append(IntToSize(packet.size()));
  expected_data.append(packet.begin(), packet.end());

  EXPECT_EQ(expected_data, sent_data_);
}

// Verify that asynchronous writes are handled correctly.
TEST_F(P2PSocketHostTcpTest, AsyncWrites) {
  base::MessageLoop message_loop;

  socket_->set_async_write(true);

  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .Times(2)
      .WillRepeatedly(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet1;
  CreateStunRequest(&packet1);

  socket_host_->Send(dest_.ip_address, packet1, options, 0);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_.ip_address, packet2, options, 0);

  base::RunLoop().RunUntilIdle();

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(IntToSize(packet2.size()));
  expected_data.append(packet2.begin(), packet2.end());

  EXPECT_EQ(expected_data, sent_data_);
}

TEST_F(P2PSocketHostTcpTest, SendDataWithPacketOptions) {
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(request_packet)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_->AppendInputData(&received_data[0], received_data.size());

  rtc::PacketOptions options;
  options.packet_time_params.rtp_sendtime_extension_id = 3;
  // Now we should be able to send any data to |dest_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  // Make it a RTP packet.
  *reinterpret_cast<uint16_t*>(&*packet.begin()) = base::HostToNet16(0x8000);
  socket_host_->Send(dest_.ip_address, packet, options, 0);

  std::string expected_data;
  expected_data.append(IntToSize(packet.size()));
  expected_data.append(packet.begin(), packet.end());

  EXPECT_EQ(expected_data, sent_data_);
}

// Verify that we can send STUN message and that they are formatted
// properly.
TEST_F(P2PSocketHostStunTcpTest, SendStunNoAuth) {
  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .Times(3)
      .WillRepeatedly(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_.ip_address, packet1, options, 0);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_.ip_address, packet2, options, 0);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest_.ip_address, packet3, options, 0);

  std::string expected_data;
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(packet2.begin(), packet2.end());
  expected_data.append(packet3.begin(), packet3.end());

  EXPECT_EQ(expected_data, sent_data_);
}

// Verify that we can receive STUN messages from the socket, and that
// the messages are parsed properly.
TEST_F(P2PSocketHostStunTcpTest, ReceiveStun) {
  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .Times(3)
      .WillRepeatedly(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_.ip_address, packet1, options, 0);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_.ip_address, packet2, options, 0);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest_.ip_address, packet3, options, 0);

  std::string received_data;
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet1)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet2)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet3)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= arraysize(step_sizes))
      step = 0;
  }
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketHostStunTcpTest, SendDataNoAuth) {
  EXPECT_CALL(sender_,
              Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnError::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest_.ip_address, packet, options, 0);

  EXPECT_EQ(0U, sent_data_.size());
}

// Verify that asynchronous writes are handled correctly.
TEST_F(P2PSocketHostStunTcpTest, AsyncWrites) {
  base::MessageLoop message_loop;

  socket_->set_async_write(true);

  EXPECT_CALL(
      sender_,
      Send(MatchMessage(static_cast<uint32_t>(P2PMsg_OnSendComplete::ID))))
      .Times(2)
      .WillRepeatedly(DoAll(DeleteArg<0>(), Return(true)));

  rtc::PacketOptions options;
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_.ip_address, packet1, options, 0);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_.ip_address, packet2, options, 0);

  base::RunLoop().RunUntilIdle();

  std::string expected_data;
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(packet2.begin(), packet2.end());

  EXPECT_EQ(expected_data, sent_data_);
}

}  // namespace content
