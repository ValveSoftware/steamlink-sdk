// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include <list>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "net/base/net_log.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_socket_pool.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestClientSocketFactory : public ClientSocketFactory {
 public:
  virtual ~TestClientSocketFactory();

  virtual scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      net::NetLog* net_log,
      const net::NetLog::Source& source) OVERRIDE;

  virtual scoped_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog*, const NetLog::Source&) OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<StreamSocket>();
  }

  virtual scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<ClientSocketHandle> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      const SSLClientSocketContext& context) OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<SSLClientSocket>();
  }

  virtual void ClearSSLSessionCache() OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  std::list<SocketDataProvider*> data_providers_;
};

struct PoolEvent {
  enum { ALLOCATE, FREE } action;
  unsigned server_index;
};

class DnsSessionTest : public testing::Test {
 public:
  void OnSocketAllocated(unsigned server_index);
  void OnSocketFreed(unsigned server_index);

 protected:
  void Initialize(unsigned num_servers);
  scoped_ptr<DnsSession::SocketLease> Allocate(unsigned server_index);
  bool DidAllocate(unsigned server_index);
  bool DidFree(unsigned server_index);
  bool NoMoreEvents();

  DnsConfig config_;
  scoped_ptr<TestClientSocketFactory> test_client_socket_factory_;
  scoped_refptr<DnsSession> session_;
  NetLog::Source source_;

 private:
  bool ExpectEvent(const PoolEvent& event);
  std::list<PoolEvent> events_;
};

class MockDnsSocketPool : public DnsSocketPool {
 public:
  MockDnsSocketPool(ClientSocketFactory* factory, DnsSessionTest* test)
     : DnsSocketPool(factory), test_(test) { }

  virtual ~MockDnsSocketPool() { }

  virtual void Initialize(
      const std::vector<IPEndPoint>* nameservers,
      NetLog* net_log) OVERRIDE {
    InitializeInternal(nameservers, net_log);
  }

  virtual scoped_ptr<DatagramClientSocket> AllocateSocket(
      unsigned server_index) OVERRIDE {
    test_->OnSocketAllocated(server_index);
    return CreateConnectedSocket(server_index);
  }

  virtual void FreeSocket(
      unsigned server_index,
      scoped_ptr<DatagramClientSocket> socket) OVERRIDE {
    test_->OnSocketFreed(server_index);
  }

 private:
  DnsSessionTest* test_;
};

void DnsSessionTest::Initialize(unsigned num_servers) {
  CHECK(num_servers < 256u);
  config_.nameservers.clear();
  IPAddressNumber dns_ip;
  bool rv = ParseIPLiteralToNumber("192.168.1.0", &dns_ip);
  EXPECT_TRUE(rv);
  for (unsigned char i = 0; i < num_servers; ++i) {
    dns_ip[3] = i;
    IPEndPoint dns_endpoint(dns_ip, dns_protocol::kDefaultPort);
    config_.nameservers.push_back(dns_endpoint);
  }

  test_client_socket_factory_.reset(new TestClientSocketFactory());

  DnsSocketPool* dns_socket_pool =
      new MockDnsSocketPool(test_client_socket_factory_.get(), this);

  session_ = new DnsSession(config_,
                            scoped_ptr<DnsSocketPool>(dns_socket_pool),
                            base::Bind(&base::RandInt),
                            NULL /* NetLog */);

  events_.clear();
}

scoped_ptr<DnsSession::SocketLease> DnsSessionTest::Allocate(
    unsigned server_index) {
  return session_->AllocateSocket(server_index, source_);
}

bool DnsSessionTest::DidAllocate(unsigned server_index) {
  PoolEvent expected_event = { PoolEvent::ALLOCATE, server_index };
  return ExpectEvent(expected_event);
}

bool DnsSessionTest::DidFree(unsigned server_index) {
  PoolEvent expected_event = { PoolEvent::FREE, server_index };
  return ExpectEvent(expected_event);
}

bool DnsSessionTest::NoMoreEvents() {
  return events_.empty();
}

void DnsSessionTest::OnSocketAllocated(unsigned server_index) {
  PoolEvent event = { PoolEvent::ALLOCATE, server_index };
  events_.push_back(event);
}

void DnsSessionTest::OnSocketFreed(unsigned server_index) {
  PoolEvent event = { PoolEvent::FREE, server_index };
  events_.push_back(event);
}

bool DnsSessionTest::ExpectEvent(const PoolEvent& expected) {
  if (events_.empty()) {
    return false;
  }

  const PoolEvent actual = events_.front();
  if ((expected.action != actual.action)
      || (expected.server_index != actual.server_index)) {
    return false;
  }
  events_.pop_front();

  return true;
}

scoped_ptr<DatagramClientSocket>
TestClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    const RandIntCallback& rand_int_cb,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  // We're not actually expecting to send or receive any data, so use the
  // simplest SocketDataProvider with no data supplied.
  SocketDataProvider* data_provider = new StaticSocketDataProvider();
  data_providers_.push_back(data_provider);
  scoped_ptr<MockUDPClientSocket> socket(
      new MockUDPClientSocket(data_provider, net_log));
  data_provider->set_socket(socket.get());
  return socket.PassAs<DatagramClientSocket>();
}

TestClientSocketFactory::~TestClientSocketFactory() {
  STLDeleteElements(&data_providers_);
}

TEST_F(DnsSessionTest, AllocateFree) {
  scoped_ptr<DnsSession::SocketLease> lease1, lease2;

  Initialize(2);
  EXPECT_TRUE(NoMoreEvents());

  lease1 = Allocate(0);
  EXPECT_TRUE(DidAllocate(0));
  EXPECT_TRUE(NoMoreEvents());

  lease2 = Allocate(1);
  EXPECT_TRUE(DidAllocate(1));
  EXPECT_TRUE(NoMoreEvents());

  lease1.reset();
  EXPECT_TRUE(DidFree(0));
  EXPECT_TRUE(NoMoreEvents());

  lease2.reset();
  EXPECT_TRUE(DidFree(1));
  EXPECT_TRUE(NoMoreEvents());
}

// Expect default calculated timeout to be within 10ms of in DnsConfig.
TEST_F(DnsSessionTest, HistogramTimeoutNormal) {
  Initialize(2);
  base::TimeDelta timeoutDelta = session_->NextTimeout(0, 0) - config_.timeout;
  EXPECT_LT(timeoutDelta.InMilliseconds(), 10);
}

// Expect short calculated timeout to be within 10ms of in DnsConfig.
TEST_F(DnsSessionTest, HistogramTimeoutShort) {
  config_.timeout = base::TimeDelta::FromMilliseconds(15);
  Initialize(2);
  base::TimeDelta timeoutDelta = session_->NextTimeout(0, 0) - config_.timeout;
  EXPECT_LT(timeoutDelta.InMilliseconds(), 10);
}

// Expect long calculated timeout to be equal to one in DnsConfig.
TEST_F(DnsSessionTest, HistogramTimeoutLong) {
  config_.timeout = base::TimeDelta::FromSeconds(15);
  Initialize(2);
  base::TimeDelta timeout = session_->NextTimeout(0, 0);
  EXPECT_EQ(config_.timeout.InMilliseconds(), timeout.InMilliseconds());
}

}  // namespace

} // namespace net
