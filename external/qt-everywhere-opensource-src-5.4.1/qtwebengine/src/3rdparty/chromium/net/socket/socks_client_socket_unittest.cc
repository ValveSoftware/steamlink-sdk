// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/dns/mock_host_resolver.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

const char kSOCKSOkRequest[] = { 0x04, 0x01, 0x00, 0x50, 127, 0, 0, 1, 0 };
const char kSOCKSOkReply[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

class SOCKSClientSocketTest : public PlatformTest {
 public:
  SOCKSClientSocketTest();
  // Create a SOCKSClientSocket on top of a MockSocket.
  scoped_ptr<SOCKSClientSocket> BuildMockSocket(
      MockRead reads[], size_t reads_count,
      MockWrite writes[], size_t writes_count,
      HostResolver* host_resolver,
      const std::string& hostname, int port,
      NetLog* net_log);
  virtual void SetUp();

 protected:
  scoped_ptr<SOCKSClientSocket> user_sock_;
  AddressList address_list_;
  // Filled in by BuildMockSocket() and owned by its return value
  // (which |user_sock| is set to).
  StreamSocket* tcp_sock_;
  TestCompletionCallback callback_;
  scoped_ptr<MockHostResolver> host_resolver_;
  scoped_ptr<SocketDataProvider> data_;
};

SOCKSClientSocketTest::SOCKSClientSocketTest()
  : host_resolver_(new MockHostResolver) {
}

// Set up platform before every test case
void SOCKSClientSocketTest::SetUp() {
  PlatformTest::SetUp();
}

scoped_ptr<SOCKSClientSocket> SOCKSClientSocketTest::BuildMockSocket(
    MockRead reads[],
    size_t reads_count,
    MockWrite writes[],
    size_t writes_count,
    HostResolver* host_resolver,
    const std::string& hostname,
    int port,
    NetLog* net_log) {

  TestCompletionCallback callback;
  data_.reset(new StaticSocketDataProvider(reads, reads_count,
                                           writes, writes_count));
  tcp_sock_ = new MockTCPClientSocket(address_list_, net_log, data_.get());

  int rv = tcp_sock_->Connect(callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(tcp_sock_->IsConnected());

  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  // |connection| takes ownership of |tcp_sock_|, but keep a
  // non-owning pointer to it.
  connection->SetSocket(scoped_ptr<StreamSocket>(tcp_sock_));
  return scoped_ptr<SOCKSClientSocket>(new SOCKSClientSocket(
      connection.Pass(),
      HostResolver::RequestInfo(HostPortPair(hostname, port)),
      DEFAULT_PRIORITY,
      host_resolver));
}

// Implementation of HostResolver that never completes its resolve request.
// We use this in the test "DisconnectWhileHostResolveInProgress" to make
// sure that the outstanding resolve request gets cancelled.
class HangingHostResolverWithCancel : public HostResolver {
 public:
  HangingHostResolverWithCancel() : outstanding_request_(NULL) {}

  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) OVERRIDE {
    DCHECK(addresses);
    DCHECK_EQ(false, callback.is_null());
    EXPECT_FALSE(HasOutstandingRequest());
    outstanding_request_ = reinterpret_cast<RequestHandle>(1);
    *out_req = outstanding_request_;
    return ERR_IO_PENDING;
  }

  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& net_log) OVERRIDE {
    NOTIMPLEMENTED();
    return ERR_UNEXPECTED;
  }

  virtual void CancelRequest(RequestHandle req) OVERRIDE {
    EXPECT_TRUE(HasOutstandingRequest());
    EXPECT_EQ(outstanding_request_, req);
    outstanding_request_ = NULL;
  }

  bool HasOutstandingRequest() {
    return outstanding_request_ != NULL;
  }

 private:
  RequestHandle outstanding_request_;

  DISALLOW_COPY_AND_ASSIGN(HangingHostResolverWithCancel);
};

// Tests a complete handshake and the disconnection.
TEST_F(SOCKSClientSocketTest, CompleteHandshake) {
  const std::string payload_write = "random data";
  const std::string payload_read = "moar random data";

  MockWrite data_writes[] = {
      MockWrite(ASYNC, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)),
      MockWrite(ASYNC, payload_write.data(), payload_write.size()) };
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKSOkReply, arraysize(kSOCKSOkReply)),
      MockRead(ASYNC, payload_read.data(), payload_read.size()) };
  CapturingNetLog log;

  user_sock_ = BuildMockSocket(data_reads, arraysize(data_reads),
                               data_writes, arraysize(data_writes),
                               host_resolver_.get(),
                               "localhost", 80,
                               &log);

  // At this state the TCP connection is completed but not the SOCKS handshake.
  EXPECT_TRUE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(
      LogContainsBeginEvent(entries, 0, NetLog::TYPE_SOCKS_CONNECT));
  EXPECT_FALSE(user_sock_->IsConnected());

  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(
      entries, -1, NetLog::TYPE_SOCKS_CONNECT));

  scoped_refptr<IOBuffer> buffer(new IOBuffer(payload_write.size()));
  memcpy(buffer->data(), payload_write.data(), payload_write.size());
  rv = user_sock_->Write(
      buffer.get(), payload_write.size(), callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_.WaitForResult();
  EXPECT_EQ(static_cast<int>(payload_write.size()), rv);

  buffer = new IOBuffer(payload_read.size());
  rv =
      user_sock_->Read(buffer.get(), payload_read.size(), callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_.WaitForResult();
  EXPECT_EQ(static_cast<int>(payload_read.size()), rv);
  EXPECT_EQ(payload_read, std::string(buffer->data(), payload_read.size()));

  user_sock_->Disconnect();
  EXPECT_FALSE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());
}

// List of responses from the socks server and the errors they should
// throw up are tested here.
TEST_F(SOCKSClientSocketTest, HandshakeFailures) {
  const struct {
    const char fail_reply[8];
    Error fail_code;
  } tests[] = {
    // Failure of the server response code
    {
      { 0x01, 0x5A, 0x00, 0x00, 0, 0, 0, 0 },
      ERR_SOCKS_CONNECTION_FAILED,
    },
    // Failure of the null byte
    {
      { 0x00, 0x5B, 0x00, 0x00, 0, 0, 0, 0 },
      ERR_SOCKS_CONNECTION_FAILED,
    },
  };

  //---------------------------------------

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    MockWrite data_writes[] = {
        MockWrite(SYNCHRONOUS, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)) };
    MockRead data_reads[] = {
        MockRead(SYNCHRONOUS, tests[i].fail_reply,
                 arraysize(tests[i].fail_reply)) };
    CapturingNetLog log;

    user_sock_ = BuildMockSocket(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes),
                                 host_resolver_.get(),
                                 "localhost", 80,
                                 &log);

    int rv = user_sock_->Connect(callback_.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    CapturingNetLog::CapturedEntryList entries;
    log.GetEntries(&entries);
    EXPECT_TRUE(LogContainsBeginEvent(
        entries, 0, NetLog::TYPE_SOCKS_CONNECT));

    rv = callback_.WaitForResult();
    EXPECT_EQ(tests[i].fail_code, rv);
    EXPECT_FALSE(user_sock_->IsConnected());
    EXPECT_TRUE(tcp_sock_->IsConnected());
    log.GetEntries(&entries);
    EXPECT_TRUE(LogContainsEndEvent(
        entries, -1, NetLog::TYPE_SOCKS_CONNECT));
  }
}

// Tests scenario when the server sends the handshake response in
// more than one packet.
TEST_F(SOCKSClientSocketTest, PartialServerReads) {
  const char kSOCKSPartialReply1[] = { 0x00 };
  const char kSOCKSPartialReply2[] = { 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
      MockWrite(ASYNC, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)) };
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKSPartialReply1, arraysize(kSOCKSPartialReply1)),
      MockRead(ASYNC, kSOCKSPartialReply2, arraysize(kSOCKSPartialReply2)) };
  CapturingNetLog log;

  user_sock_ = BuildMockSocket(data_reads, arraysize(data_reads),
                               data_writes, arraysize(data_writes),
                               host_resolver_.get(),
                               "localhost", 80,
                               &log);

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(
      entries, -1, NetLog::TYPE_SOCKS_CONNECT));
}

// Tests scenario when the client sends the handshake request in
// more than one packet.
TEST_F(SOCKSClientSocketTest, PartialClientWrites) {
  const char kSOCKSPartialRequest1[] = { 0x04, 0x01 };
  const char kSOCKSPartialRequest2[] = { 0x00, 0x50, 127, 0, 0, 1, 0 };

  MockWrite data_writes[] = {
      MockWrite(ASYNC, arraysize(kSOCKSPartialRequest1)),
      // simulate some empty writes
      MockWrite(ASYNC, 0),
      MockWrite(ASYNC, 0),
      MockWrite(ASYNC, kSOCKSPartialRequest2,
                arraysize(kSOCKSPartialRequest2)) };
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKSOkReply, arraysize(kSOCKSOkReply)) };
  CapturingNetLog log;

  user_sock_ = BuildMockSocket(data_reads, arraysize(data_reads),
                               data_writes, arraysize(data_writes),
                               host_resolver_.get(),
                               "localhost", 80,
                               &log);

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(
      entries, -1, NetLog::TYPE_SOCKS_CONNECT));
}

// Tests the case when the server sends a smaller sized handshake data
// and closes the connection.
TEST_F(SOCKSClientSocketTest, FailedSocketRead) {
  MockWrite data_writes[] = {
      MockWrite(ASYNC, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)) };
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKSOkReply, arraysize(kSOCKSOkReply) - 2),
      // close connection unexpectedly
      MockRead(SYNCHRONOUS, 0) };
  CapturingNetLog log;

  user_sock_ = BuildMockSocket(data_reads, arraysize(data_reads),
                               data_writes, arraysize(data_writes),
                               host_resolver_.get(),
                               "localhost", 80,
                               &log);

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
  EXPECT_FALSE(user_sock_->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(
      entries, -1, NetLog::TYPE_SOCKS_CONNECT));
}

// Tries to connect to an unknown hostname. Should fail rather than
// falling back to SOCKS4a.
TEST_F(SOCKSClientSocketTest, FailedDNS) {
  const char hostname[] = "unresolved.ipv4.address";

  host_resolver_->rules()->AddSimulatedFailure(hostname);

  CapturingNetLog log;

  user_sock_ = BuildMockSocket(NULL, 0,
                               NULL, 0,
                               host_resolver_.get(),
                               hostname, 80,
                               &log);

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
  EXPECT_FALSE(user_sock_->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(
      entries, -1, NetLog::TYPE_SOCKS_CONNECT));
}

// Calls Disconnect() while a host resolve is in progress. The outstanding host
// resolve should be cancelled.
TEST_F(SOCKSClientSocketTest, DisconnectWhileHostResolveInProgress) {
  scoped_ptr<HangingHostResolverWithCancel> hanging_resolver(
    new HangingHostResolverWithCancel());

  // Doesn't matter what the socket data is, we will never use it -- garbage.
  MockWrite data_writes[] = { MockWrite(SYNCHRONOUS, "", 0) };
  MockRead data_reads[] = { MockRead(SYNCHRONOUS, "", 0) };

  user_sock_ = BuildMockSocket(data_reads, arraysize(data_reads),
                               data_writes, arraysize(data_writes),
                               hanging_resolver.get(),
                               "foo", 80,
                               NULL);

  // Start connecting (will get stuck waiting for the host to resolve).
  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnectedAndIdle());

  // The host resolver should have received the resolve request.
  EXPECT_TRUE(hanging_resolver->HasOutstandingRequest());

  // Disconnect the SOCKS socket -- this should cancel the outstanding resolve.
  user_sock_->Disconnect();

  EXPECT_FALSE(hanging_resolver->HasOutstandingRequest());

  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnectedAndIdle());
}

}  // namespace net
