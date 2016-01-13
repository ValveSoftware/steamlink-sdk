// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some tests for TCPClientSocket.
// transport_client_socket_unittest.cc contans some other tests that
// are common for TCP and other types of sockets.

#include "net/socket/tcp_client_socket.h"

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Try binding a socket to loopback interface and verify that we can
// still connect to a server on the same interface.
TEST(TCPClientSocketTest, BindLoopbackToLoopback) {
  IPAddressNumber lo_address;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &lo_address));

  TCPServerSocket server(NULL, NetLog::Source());
  ASSERT_EQ(OK, server.Listen(IPEndPoint(lo_address, 0), 1));
  IPEndPoint server_address;
  ASSERT_EQ(OK, server.GetLocalAddress(&server_address));

  TCPClientSocket socket(AddressList(server_address), NULL, NetLog::Source());

  EXPECT_EQ(OK, socket.Bind(IPEndPoint(lo_address, 0)));

  IPEndPoint local_address_result;
  EXPECT_EQ(OK, socket.GetLocalAddress(&local_address_result));
  EXPECT_EQ(lo_address, local_address_result.address());

  TestCompletionCallback connect_callback;
  EXPECT_EQ(ERR_IO_PENDING, socket.Connect(connect_callback.callback()));

  TestCompletionCallback accept_callback;
  scoped_ptr<StreamSocket> accepted_socket;
  int result = server.Accept(&accepted_socket, accept_callback.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback.WaitForResult();
  ASSERT_EQ(OK, result);

  EXPECT_EQ(OK, connect_callback.WaitForResult());

  EXPECT_TRUE(socket.IsConnected());
  socket.Disconnect();
  EXPECT_FALSE(socket.IsConnected());
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            socket.GetLocalAddress(&local_address_result));
}

// Try to bind socket to the loopback interface and connect to an
// external address, verify that connection fails.
TEST(TCPClientSocketTest, BindLoopbackToExternal) {
  IPAddressNumber external_ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("72.14.213.105", &external_ip));
  TCPClientSocket socket(AddressList::CreateFromIPAddress(external_ip, 80),
                         NULL, NetLog::Source());

  IPAddressNumber lo_address;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &lo_address));
  EXPECT_EQ(OK, socket.Bind(IPEndPoint(lo_address, 0)));

  TestCompletionCallback connect_callback;
  int result = socket.Connect(connect_callback.callback());
  if (result == ERR_IO_PENDING)
    result = connect_callback.WaitForResult();

  // We may get different errors here on different system, but
  // connect() is not expected to succeed.
  EXPECT_NE(OK, result);
}

// Bind a socket to the IPv4 loopback interface and try to connect to
// the IPv6 loopback interface, verify that connection fails.
TEST(TCPClientSocketTest, BindLoopbackToIPv6) {
  IPAddressNumber ipv6_lo_ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("::1", &ipv6_lo_ip));
  TCPServerSocket server(NULL, NetLog::Source());
  int listen_result = server.Listen(IPEndPoint(ipv6_lo_ip, 0), 1);
  if (listen_result != OK) {
    LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is disabled."
        " Skipping the test";
    return;
  }

  IPEndPoint server_address;
  ASSERT_EQ(OK, server.GetLocalAddress(&server_address));
  TCPClientSocket socket(AddressList(server_address), NULL, NetLog::Source());

  IPAddressNumber ipv4_lo_ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &ipv4_lo_ip));
  EXPECT_EQ(OK, socket.Bind(IPEndPoint(ipv4_lo_ip, 0)));

  TestCompletionCallback connect_callback;
  int result = socket.Connect(connect_callback.callback());
  if (result == ERR_IO_PENDING)
    result = connect_callback.WaitForResult();

  EXPECT_NE(OK, result);
}

}  // namespace

}  // namespace net
