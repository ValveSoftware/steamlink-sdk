// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_MOCK_TCP_CLIENT_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_MOCK_TCP_CLIENT_SOCKET_H_

#include "net/socket/tcp_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class MockTCPClientSocket : public net::TCPClientSocket {
 public:
  MockTCPClientSocket();
  virtual ~MockTCPClientSocket();

  // Copied from MockTCPClientSocket in blimp/net/test_common.h
  MOCK_METHOD3(Read, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD1(SetReceiveBufferSize, int(int32_t));
  MOCK_METHOD1(SetSendBufferSize, int(int32_t));
  MOCK_METHOD1(Connect, int(const net::CompletionCallback&));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectedAndIdle, bool());
  MOCK_CONST_METHOD1(GetPeerAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD1(GetLocalAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD0(NetLog, const net::BoundNetLog&());
  MOCK_METHOD0(SetSubresourceSpeculation, void());
  MOCK_METHOD0(SetOmniboxSpeculation, void());
  MOCK_CONST_METHOD0(WasEverUsed, bool());
  MOCK_CONST_METHOD0(UsingTCPFastOpen, bool());
  MOCK_CONST_METHOD0(NumBytesRead, int64_t());
  MOCK_CONST_METHOD0(GetConnectTimeMicros, base::TimeDelta());
  MOCK_CONST_METHOD0(WasNpnNegotiated, bool());
  MOCK_CONST_METHOD0(GetNegotiatedProtocol, net::NextProto());
  MOCK_METHOD1(GetSSLInfo, bool(net::SSLInfo*));
  MOCK_CONST_METHOD1(GetConnectionAttempts, void(net::ConnectionAttempts*));
  MOCK_METHOD0(ClearConnectionAttempts, void());
  MOCK_METHOD1(AddConnectionAttempts, void(const net::ConnectionAttempts&));
  MOCK_CONST_METHOD0(GetTotalReceivedBytes, int64_t());

  // Methods specific to MockTCPClientSocket
  MOCK_METHOD1(Bind, int(const net::IPEndPoint&));
  MOCK_METHOD2(SetKeepAlive, bool(bool, int));
  MOCK_METHOD1(SetNoDelay, bool(bool));
};

MockTCPClientSocket::MockTCPClientSocket()
    : TCPClientSocket(net::AddressList(),
                      nullptr,
                      nullptr,
                      net::NetLog::Source()) {}
MockTCPClientSocket::~MockTCPClientSocket() {}

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_MOCK_TCP_CLIENT_SOCKET_H_
