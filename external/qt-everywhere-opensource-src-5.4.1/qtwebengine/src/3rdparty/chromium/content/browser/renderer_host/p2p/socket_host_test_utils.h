// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TEST_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TEST_UTILS_H_

#include <vector>

#include "content/common/p2p_messages.h"
#include "ipc/ipc_sender.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestLocalIpAddress[] = "123.44.22.4";
const char kTestIpAddress1[] = "123.44.22.31";
const int kTestPort1 = 234;
const char kTestIpAddress2[] = "133.11.22.33";
const int kTestPort2 = 543;

class MockIPCSender : public IPC::Sender {
 public:
  MockIPCSender();
  virtual ~MockIPCSender();

  MOCK_METHOD1(Send, bool(IPC::Message* msg));
};

class FakeSocket : public net::StreamSocket {
 public:
  FakeSocket(std::string* written_data);
  virtual ~FakeSocket();

  void set_async_write(bool async_write) { async_write_ = async_write; }
  void AppendInputData(const char* data, int data_size);
  int input_pos() const { return input_pos_; }
  bool read_pending() const { return read_pending_; }
  void SetPeerAddress(const net::IPEndPoint& peer_address);
  void SetLocalAddress(const net::IPEndPoint& local_address);

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;
  virtual int Connect(const net::CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual net::NextProto GetNegotiatedProtocol() const OVERRIDE;
  virtual bool GetSSLInfo(net::SSLInfo* ssl_info) OVERRIDE;

 private:
  void DoAsyncWrite(scoped_refptr<net::IOBuffer> buf, int buf_len,
                    const net::CompletionCallback& callback);

  bool read_pending_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;

  std::string input_data_;
  int input_pos_;

  std::string* written_data_;
  bool async_write_;
  bool write_pending_;

  net::IPEndPoint peer_address_;
  net::IPEndPoint local_address_;

  net::BoundNetLog net_log_;
};

void CreateRandomPacket(std::vector<char>* packet);
void CreateStunRequest(std::vector<char>* packet);
void CreateStunResponse(std::vector<char>* packet);
void CreateStunError(std::vector<char>* packet);

net::IPEndPoint ParseAddress(const std::string ip_str, int port);

MATCHER_P(MatchMessage, type, "") {
  return arg->type() == type;
}

MATCHER_P(MatchPacketMessage, packet_content, "") {
  if (arg->type() != P2PMsg_OnDataReceived::ID)
    return false;
  P2PMsg_OnDataReceived::Param params;
  P2PMsg_OnDataReceived::Read(arg, &params);
  return params.c == packet_content;
}

MATCHER_P(MatchIncomingSocketMessage, address, "") {
  if (arg->type() != P2PMsg_OnIncomingTcpConnection::ID)
    return false;
  P2PMsg_OnIncomingTcpConnection::Param params;
  P2PMsg_OnIncomingTcpConnection::Read(
      arg, &params);
  return params.b == address;
}

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TEST_UTILS_H_
