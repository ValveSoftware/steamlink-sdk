// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>

#include "base/message_loop/message_loop.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/protocol_control.pb.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_stats.h"
#include "blimp/net/tcp_client_transport.h"
#include "blimp/net/tcp_engine_transport.h"
#include "blimp/net/test_common.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace blimp {

namespace {

// Integration test for TCPEngineTransport and TCPClientTransport.
class TCPTransportTest : public testing::Test {
 protected:
  TCPTransportTest()
      : local_address_(net::IPAddress(127, 0, 0, 1), 0),
        engine_(local_address_, nullptr),
        read_buffer_(new net::GrowableIOBuffer) {
    size_t buf_size = std::max(payload_1_.size(), payload_2_.size());
    write_buffer_ = make_scoped_refptr(
        new net::DrainableIOBuffer(new net::IOBuffer(buf_size), buf_size));
    read_buffer_->SetCapacity(buf_size);
  }

  net::IPEndPoint GetLocalEndpoint() const {
    net::IPEndPoint local_address;
    engine_.GetLocalAddress(&local_address);
    return local_address;
  }

  std::string payload_1_ = "foo";
  std::string payload_2_ = "bar";
  base::MessageLoopForIO message_loop_;
  net::IPEndPoint local_address_;
  TCPEngineTransport engine_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
};

TEST_F(TCPTransportTest, Connect) {
  net::TestCompletionCallback accept_callback;
  engine_.Connect(accept_callback.callback());

  net::TestCompletionCallback connect_callback;
  TCPClientTransport client(GetLocalEndpoint(), nullptr);
  client.Connect(connect_callback.callback());

  EXPECT_EQ(net::OK, connect_callback.WaitForResult());
  EXPECT_EQ(net::OK, accept_callback.WaitForResult());
  EXPECT_NE(nullptr, client.TakeMessagePort());
}

TEST_F(TCPTransportTest, TwoClientConnections) {
  net::TestCompletionCallback accept_callback1;
  engine_.Connect(accept_callback1.callback());

  net::TestCompletionCallback connect_callback1;
  TCPClientTransport client(GetLocalEndpoint(), nullptr);
  client.Connect(connect_callback1.callback());
  EXPECT_EQ(net::OK, connect_callback1.WaitForResult());
  EXPECT_EQ(net::OK, accept_callback1.WaitForResult());
  EXPECT_NE(nullptr, engine_.TakeMessagePort());

  net::TestCompletionCallback accept_callback2;
  engine_.Connect(accept_callback2.callback());

  net::TestCompletionCallback connect_callback2;
  TCPClientTransport client2(GetLocalEndpoint(), nullptr);
  client2.Connect(connect_callback2.callback());
  EXPECT_EQ(net::OK, connect_callback2.WaitForResult());
  EXPECT_EQ(net::OK, accept_callback2.WaitForResult());
  EXPECT_NE(nullptr, engine_.TakeMessagePort());
}

TEST_F(TCPTransportTest, ExchangeMessages) {
  // Start the Engine transport and connect a client to it.
  net::TestCompletionCallback accept_callback;
  engine_.Connect(accept_callback.callback());
  net::TestCompletionCallback client_connect_callback;
  TCPClientTransport client(GetLocalEndpoint(), nullptr);
  client.Connect(client_connect_callback.callback());
  EXPECT_EQ(net::OK, accept_callback.WaitForResult());
  EXPECT_EQ(net::OK, client_connect_callback.WaitForResult());

  std::unique_ptr<MessagePort> engine_message_port = engine_.TakeMessagePort();
  std::unique_ptr<MessagePort> clientmessage_port = client.TakeMessagePort();

  // Engine sends payload_1_ to client.
  net::TestCompletionCallback read_cb1;
  net::TestCompletionCallback write_cb1;
  memcpy(write_buffer_->data(), payload_1_.data(), payload_1_.size());
  engine_message_port->writer()->WritePacket(write_buffer_,
                                             write_cb1.callback());
  clientmessage_port->reader()->ReadPacket(read_buffer_, read_cb1.callback());
  EXPECT_EQ(payload_1_.size(), static_cast<size_t>(read_cb1.WaitForResult()));
  EXPECT_EQ(net::OK, write_cb1.WaitForResult());
  EXPECT_TRUE(
      BufferStartsWith(read_buffer_.get(), payload_1_.size(), payload_1_));

  // Client sends payload_2_ to engine.
  net::TestCompletionCallback read_cb2;
  net::TestCompletionCallback write_cb2;
  memcpy(write_buffer_->data(), payload_2_.data(), payload_2_.size());
  clientmessage_port->writer()->WritePacket(write_buffer_,
                                            write_cb2.callback());
  engine_message_port->reader()->ReadPacket(read_buffer_, read_cb2.callback());
  EXPECT_EQ(payload_2_.size(), static_cast<size_t>(read_cb2.WaitForResult()));
  EXPECT_EQ(net::OK, write_cb2.WaitForResult());
  EXPECT_TRUE(
      BufferStartsWith(read_buffer_.get(), payload_2_.size(), payload_2_));
}

}  // namespace

}  // namespace blimp
