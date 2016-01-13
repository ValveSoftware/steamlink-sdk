// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "net/base/capturing_net_log.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using internal::ClientSocketPoolBaseHelper;

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const net::RequestPriority kDefaultPriority = LOW;

// Make sure |handle| sets load times correctly when it has been assigned a
// reused socket.
void TestLoadTimingInfoConnectedReused(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  // Only pass true in as |is_reused|, as in general, HttpStream types should
  // have stricter concepts of reuse than socket pools.
  EXPECT_TRUE(handle.GetLoadTimingInfo(true, &load_timing_info));

  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Make sure |handle| sets load times correctly when it has been assigned a
// fresh socket.  Also runs TestLoadTimingInfoConnectedReused, since the owner
// of a connection where |is_reused| is false may consider the connection
// reused.
void TestLoadTimingInfoConnectedNotReused(const ClientSocketHandle& handle) {
  EXPECT_FALSE(handle.is_reused());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);

  TestLoadTimingInfoConnectedReused(handle);
}

void SetIPv4Address(IPEndPoint* address) {
  IPAddressNumber number;
  CHECK(ParseIPLiteralToNumber("1.1.1.1", &number));
  *address = IPEndPoint(number, 80);
}

void SetIPv6Address(IPEndPoint* address) {
  IPAddressNumber number;
  CHECK(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  *address = IPEndPoint(number, 80);
}

class MockClientSocket : public StreamSocket {
 public:
  MockClientSocket(const AddressList& addrlist, net::NetLog* net_log)
      : connected_(false),
        addrlist_(addrlist),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  }

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    connected_ = true;
    return OK;
  }
  virtual void Disconnect() OVERRIDE {
    connected_ = false;
  }
  virtual bool IsConnected() const OVERRIDE {
    return connected_;
  }
  virtual bool IsConnectedAndIdle() const OVERRIDE {
    return connected_;
  }
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    if (!connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.front().GetFamily() == ADDRESS_FAMILY_IPV4)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}
  virtual bool WasEverUsed() const OVERRIDE { return false; }
  virtual bool UsingTCPFastOpen() const OVERRIDE { return false; }
  virtual bool WasNpnNegotiated() const OVERRIDE {
    return false;
  }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {
    return false;
  }

  // Socket implementation.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return OK; }
  virtual int SetSendBufferSize(int32 size) OVERRIDE { return OK; }

 private:
  bool connected_;
  const AddressList addrlist_;
  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocket);
};

class MockFailingClientSocket : public StreamSocket {
 public:
  MockFailingClientSocket(const AddressList& addrlist, net::NetLog* net_log)
      : addrlist_(addrlist),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  }

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    return ERR_CONNECTION_FAILED;
  }

  virtual void Disconnect() OVERRIDE {}

  virtual bool IsConnected() const OVERRIDE {
    return false;
  }
  virtual bool IsConnectedAndIdle() const OVERRIDE {
    return false;
  }
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}
  virtual bool WasEverUsed() const OVERRIDE { return false; }
  virtual bool UsingTCPFastOpen() const OVERRIDE { return false; }
  virtual bool WasNpnNegotiated() const OVERRIDE {
    return false;
  }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {
    return false;
  }

  // Socket implementation.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return OK; }
  virtual int SetSendBufferSize(int32 size) OVERRIDE { return OK; }

 private:
  const AddressList addrlist_;
  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(MockFailingClientSocket);
};

class MockPendingClientSocket : public StreamSocket {
 public:
  // |should_connect| indicates whether the socket should successfully complete
  // or fail.
  // |should_stall| indicates that this socket should never connect.
  // |delay_ms| is the delay, in milliseconds, before simulating a connect.
  MockPendingClientSocket(
      const AddressList& addrlist,
      bool should_connect,
      bool should_stall,
      base::TimeDelta delay,
      net::NetLog* net_log)
      : should_connect_(should_connect),
        should_stall_(should_stall),
        delay_(delay),
        is_connected_(false),
        addrlist_(addrlist),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
        weak_factory_(this) {
  }

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MockPendingClientSocket::DoCallback,
                   weak_factory_.GetWeakPtr(), callback),
        delay_);
    return ERR_IO_PENDING;
  }

  virtual void Disconnect() OVERRIDE {}

  virtual bool IsConnected() const OVERRIDE {
    return is_connected_;
  }
  virtual bool IsConnectedAndIdle() const OVERRIDE {
    return is_connected_;
  }
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    if (!is_connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.front().GetFamily() == ADDRESS_FAMILY_IPV4)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}
  virtual bool WasEverUsed() const OVERRIDE { return false; }
  virtual bool UsingTCPFastOpen() const OVERRIDE { return false; }
  virtual bool WasNpnNegotiated() const OVERRIDE {
    return false;
  }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {
    return false;
  }

  // Socket implementation.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return OK; }
  virtual int SetSendBufferSize(int32 size) OVERRIDE { return OK; }

 private:
  void DoCallback(const CompletionCallback& callback) {
    if (should_stall_)
      return;

    if (should_connect_) {
      is_connected_ = true;
      callback.Run(OK);
    } else {
      is_connected_ = false;
      callback.Run(ERR_CONNECTION_FAILED);
    }
  }

  bool should_connect_;
  bool should_stall_;
  base::TimeDelta delay_;
  bool is_connected_;
  const AddressList addrlist_;
  BoundNetLog net_log_;

  base::WeakPtrFactory<MockPendingClientSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockPendingClientSocket);
};

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  enum ClientSocketType {
    MOCK_CLIENT_SOCKET,
    MOCK_FAILING_CLIENT_SOCKET,
    MOCK_PENDING_CLIENT_SOCKET,
    MOCK_PENDING_FAILING_CLIENT_SOCKET,
    // A delayed socket will pause before connecting through the message loop.
    MOCK_DELAYED_CLIENT_SOCKET,
    // A stalled socket that never connects at all.
    MOCK_STALLED_CLIENT_SOCKET,
  };

  explicit MockClientSocketFactory(NetLog* net_log)
      : net_log_(net_log), allocation_count_(0),
        client_socket_type_(MOCK_CLIENT_SOCKET), client_socket_types_(NULL),
        client_socket_index_(0), client_socket_index_max_(0),
        delay_(base::TimeDelta::FromMilliseconds(
            ClientSocketPool::kMaxConnectRetryIntervalMs)) {}

  virtual scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE {
    NOTREACHED();
    return scoped_ptr<DatagramClientSocket>();
  }

  virtual scoped_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* /* net_log */,
      const NetLog::Source& /* source */) OVERRIDE {
    allocation_count_++;

    ClientSocketType type = client_socket_type_;
    if (client_socket_types_ &&
        client_socket_index_ < client_socket_index_max_) {
      type = client_socket_types_[client_socket_index_++];
    }

    switch (type) {
      case MOCK_CLIENT_SOCKET:
        return scoped_ptr<StreamSocket>(
            new MockClientSocket(addresses, net_log_));
      case MOCK_FAILING_CLIENT_SOCKET:
        return scoped_ptr<StreamSocket>(
            new MockFailingClientSocket(addresses, net_log_));
      case MOCK_PENDING_CLIENT_SOCKET:
        return scoped_ptr<StreamSocket>(
            new MockPendingClientSocket(
                addresses, true, false, base::TimeDelta(), net_log_));
      case MOCK_PENDING_FAILING_CLIENT_SOCKET:
        return scoped_ptr<StreamSocket>(
            new MockPendingClientSocket(
                addresses, false, false, base::TimeDelta(), net_log_));
      case MOCK_DELAYED_CLIENT_SOCKET:
        return scoped_ptr<StreamSocket>(
            new MockPendingClientSocket(
                addresses, true, false, delay_, net_log_));
      case MOCK_STALLED_CLIENT_SOCKET:
        return scoped_ptr<StreamSocket>(
            new MockPendingClientSocket(
                addresses, true, true, base::TimeDelta(), net_log_));
      default:
        NOTREACHED();
        return scoped_ptr<StreamSocket>(
            new MockClientSocket(addresses, net_log_));
    }
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

  int allocation_count() const { return allocation_count_; }

  // Set the default ClientSocketType.
  void set_client_socket_type(ClientSocketType type) {
    client_socket_type_ = type;
  }

  // Set a list of ClientSocketTypes to be used.
  void set_client_socket_types(ClientSocketType* type_list, int num_types) {
    DCHECK_GT(num_types, 0);
    client_socket_types_ = type_list;
    client_socket_index_ = 0;
    client_socket_index_max_ = num_types;
  }

  void set_delay(base::TimeDelta delay) { delay_ = delay; }

 private:
  NetLog* net_log_;
  int allocation_count_;
  ClientSocketType client_socket_type_;
  ClientSocketType* client_socket_types_;
  int client_socket_index_;
  int client_socket_index_max_;
  base::TimeDelta delay_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocketFactory);
};

class TransportClientSocketPoolTest : public testing::Test {
 protected:
  TransportClientSocketPoolTest()
      : connect_backup_jobs_enabled_(
            ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(true)),
        params_(
            new TransportSocketParams(HostPortPair("www.google.com", 80),
                                      false, false,
                                      OnHostResolutionCallback())),
        histograms_(new ClientSocketPoolHistograms("TCPUnitTest")),
        host_resolver_(new MockHostResolver),
        client_socket_factory_(&net_log_),
        pool_(kMaxSockets,
              kMaxSocketsPerGroup,
              histograms_.get(),
              host_resolver_.get(),
              &client_socket_factory_,
              NULL) {
  }

  virtual ~TransportClientSocketPoolTest() {
    internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        connect_backup_jobs_enabled_);
  }

  int StartRequest(const std::string& group_name, RequestPriority priority) {
    scoped_refptr<TransportSocketParams> params(new TransportSocketParams(
        HostPortPair("www.google.com", 80), false, false,
        OnHostResolutionCallback()));
    return test_base_.StartRequestUsingPool(
        &pool_, group_name, priority, params);
  }

  int GetOrderOfRequest(size_t index) {
    return test_base_.GetOrderOfRequest(index);
  }

  bool ReleaseOneConnection(ClientSocketPoolTest::KeepAlive keep_alive) {
    return test_base_.ReleaseOneConnection(keep_alive);
  }

  void ReleaseAllConnections(ClientSocketPoolTest::KeepAlive keep_alive) {
    test_base_.ReleaseAllConnections(keep_alive);
  }

  ScopedVector<TestSocketRequest>* requests() { return test_base_.requests(); }
  size_t completion_count() const { return test_base_.completion_count(); }

  bool connect_backup_jobs_enabled_;
  CapturingNetLog net_log_;
  scoped_refptr<TransportSocketParams> params_;
  scoped_ptr<ClientSocketPoolHistograms> histograms_;
  scoped_ptr<MockHostResolver> host_resolver_;
  MockClientSocketFactory client_socket_factory_;
  TransportClientSocketPool pool_;
  ClientSocketPoolTest test_base_;

  DISALLOW_COPY_AND_ASSIGN(TransportClientSocketPoolTest);
};

TEST(TransportConnectJobTest, MakeAddrListStartWithIPv4) {
  IPAddressNumber ip_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("192.168.1.1", &ip_number));
  IPEndPoint addrlist_v4_1(ip_number, 80);
  ASSERT_TRUE(ParseIPLiteralToNumber("192.168.1.2", &ip_number));
  IPEndPoint addrlist_v4_2(ip_number, 80);
  ASSERT_TRUE(ParseIPLiteralToNumber("2001:4860:b006::64", &ip_number));
  IPEndPoint addrlist_v6_1(ip_number, 80);
  ASSERT_TRUE(ParseIPLiteralToNumber("2001:4860:b006::66", &ip_number));
  IPEndPoint addrlist_v6_2(ip_number, 80);

  AddressList addrlist;

  // Test 1: IPv4 only.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(2u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());

  // Test 2: IPv6 only.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(2u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[1].GetFamily());

  // Test 3: IPv4 then IPv6.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());

  // Test 4: IPv6, IPv4, IPv6, IPv4.  Expect first IPv6 moved to the end.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v6_2);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());

  // Test 5: IPv6, IPv6, IPv4, IPv4.  Expect first two IPv6's moved to the end.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());
}

TEST_F(TransportClientSocketPoolTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoConnectedNotReused(handle);
}

// Make sure that TransportConnectJob passes on its priority to its
// HostResolver request on Init.
TEST_F(TransportClientSocketPoolTest, SetResolvePriorityOnInit) {
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    RequestPriority priority = static_cast<RequestPriority>(i);
    TestCompletionCallback callback;
    ClientSocketHandle handle;
    EXPECT_EQ(ERR_IO_PENDING,
              handle.Init("a", params_, priority, callback.callback(), &pool_,
                          BoundNetLog()));
    EXPECT_EQ(priority, host_resolver_->last_request_priority());
  }
}

TEST_F(TransportClientSocketPoolTest, InitHostResolutionFailure) {
  host_resolver_->rules()->AddSimulatedFailure("unresolvable.host.name");
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  HostPortPair host_port_pair("unresolvable.host.name", 80);
  scoped_refptr<TransportSocketParams> dest(new TransportSocketParams(
      host_port_pair, false, false,
      OnHostResolutionCallback()));
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", dest, kDefaultPriority, callback.callback(),
                        &pool_, BoundNetLog()));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback.WaitForResult());
}

TEST_F(TransportClientSocketPoolTest, InitConnectionFailure) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", params_, kDefaultPriority, callback.callback(),
                        &pool_, BoundNetLog()));
  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());

  // Make the host resolutions complete synchronously this time.
  host_resolver_->set_synchronous_mode(true);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            handle.Init("a", params_, kDefaultPriority, callback.callback(),
                        &pool_, BoundNetLog()));
}

TEST_F(TransportClientSocketPoolTest, PendingRequests) {
  // First request finishes asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  // Rest of them finish synchronously, until we reach the per-group limit.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // The rest are pending since we've used all active sockets.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());

  // One initial asynchronous request and then 10 pending requests.
  EXPECT_EQ(11U, completion_count());

  // First part of requests, all with the same priority, finishes in FIFO order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));

  // Make sure that rest of the requests complete in the order of priority.
  EXPECT_EQ(7, GetOrderOfRequest(7));
  EXPECT_EQ(14, GetOrderOfRequest(8));
  EXPECT_EQ(15, GetOrderOfRequest(9));
  EXPECT_EQ(10, GetOrderOfRequest(10));
  EXPECT_EQ(13, GetOrderOfRequest(11));
  EXPECT_EQ(8, GetOrderOfRequest(12));
  EXPECT_EQ(16, GetOrderOfRequest(13));
  EXPECT_EQ(11, GetOrderOfRequest(14));
  EXPECT_EQ(12, GetOrderOfRequest(15));
  EXPECT_EQ(9, GetOrderOfRequest(16));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(17));
}

TEST_F(TransportClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  // First request finishes asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  // Rest of them finish synchronously, until we reach the per-group limit.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // The rest are pending since we've used all active sockets.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  // The pending requests should finish successfully.
  EXPECT_EQ(OK, (*requests())[6]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[7]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[8]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[9]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[10]->WaitForResult());

  EXPECT_EQ(static_cast<int>(requests()->size()),
            client_socket_factory_.allocation_count());

  // First asynchronous request, and then last 5 pending requests.
  EXPECT_EQ(6U, completion_count());
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending host resolution will eventually complete, and destroy the
// ClientSocketPool which will crash if the group was not cleared properly.
TEST_F(TransportClientSocketPoolTest, CancelRequestClearGroup) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", params_, kDefaultPriority, callback.callback(),
                        &pool_, BoundNetLog()));
  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, TwoRequestsCancelOne) {
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;

  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", params_, kDefaultPriority, callback.callback(),
                        &pool_, BoundNetLog()));
  EXPECT_EQ(ERR_IO_PENDING,
            handle2.Init("a", params_, kDefaultPriority, callback2.callback(),
                         &pool_, BoundNetLog()));

  handle.Reset();

  EXPECT_EQ(OK, callback2.WaitForResult());
  handle2.Reset();
}

TEST_F(TransportClientSocketPoolTest, ConnectCancelConnect) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", params_, kDefaultPriority, callback.callback(),
                        &pool_, BoundNetLog()));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", params_, kDefaultPriority, callback2.callback(),
                        &pool_, BoundNetLog()));

  host_resolver_->set_synchronous_mode(true);
  // At this point, handle has two ConnectingSockets out for it.  Due to the
  // setting the mock resolver into synchronous mode, the host resolution for
  // both will return in the same loop of the MessageLoop.  The client socket
  // is a pending socket, so the Connect() will asynchronously complete on the
  // next loop of the MessageLoop.  That means that the first
  // ConnectingSocket will enter OnIOComplete, and then the second one will.
  // If the first one is not cancelled, it will advance the load state, and
  // then the second one will crash.

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, CancelRequest) {
  // First request finishes asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // Reached per-group limit, queue up requests.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));

  // Cancel a request.
  size_t index_to_cancel = kMaxSocketsPerGroup + 2;
  EXPECT_FALSE((*requests())[index_to_cancel]->handle()->is_initialized());
  (*requests())[index_to_cancel]->handle()->Reset();

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(14, GetOrderOfRequest(7));
  EXPECT_EQ(7, GetOrderOfRequest(8));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound,
            GetOrderOfRequest(9));  // Canceled request.
  EXPECT_EQ(9, GetOrderOfRequest(10));
  EXPECT_EQ(10, GetOrderOfRequest(11));
  EXPECT_EQ(11, GetOrderOfRequest(12));
  EXPECT_EQ(8, GetOrderOfRequest(13));
  EXPECT_EQ(12, GetOrderOfRequest(14));
  EXPECT_EQ(13, GetOrderOfRequest(15));
  EXPECT_EQ(15, GetOrderOfRequest(16));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(17));
}

class RequestSocketCallback : public TestCompletionCallbackBase {
 public:
  RequestSocketCallback(ClientSocketHandle* handle,
                        TransportClientSocketPool* pool)
      : handle_(handle),
        pool_(pool),
        within_callback_(false),
        callback_(base::Bind(&RequestSocketCallback::OnComplete,
                             base::Unretained(this))) {
  }

  virtual ~RequestSocketCallback() {}

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    SetResult(result);
    ASSERT_EQ(OK, result);

    if (!within_callback_) {
      // Don't allow reuse of the socket.  Disconnect it and then release it and
      // run through the MessageLoop once to get it completely released.
      handle_->socket()->Disconnect();
      handle_->Reset();
      {
        base::MessageLoop::ScopedNestableTaskAllower allow(
            base::MessageLoop::current());
        base::MessageLoop::current()->RunUntilIdle();
      }
      within_callback_ = true;
      scoped_refptr<TransportSocketParams> dest(new TransportSocketParams(
          HostPortPair("www.google.com", 80), false, false,
          OnHostResolutionCallback()));
      int rv = handle_->Init("a", dest, LOWEST, callback(), pool_,
                             BoundNetLog());
      EXPECT_EQ(OK, rv);
    }
  }

  ClientSocketHandle* const handle_;
  TransportClientSocketPool* const pool_;
  bool within_callback_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(RequestSocketCallback);
};

TEST_F(TransportClientSocketPoolTest, RequestTwice) {
  ClientSocketHandle handle;
  RequestSocketCallback callback(&handle, &pool_);
  scoped_refptr<TransportSocketParams> dest(new TransportSocketParams(
      HostPortPair("www.google.com", 80), false, false,
      OnHostResolutionCallback()));
  int rv = handle.Init("a", dest, LOWEST, callback.callback(), &pool_,
                       BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  // The callback is going to request "www.google.com". We want it to complete
  // synchronously this time.
  host_resolver_->set_synchronous_mode(true);

  EXPECT_EQ(OK, callback.WaitForResult());

  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(TransportClientSocketPoolTest, CancelActiveRequestWithPendingRequests) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);

  // Queue up all the requests
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Now, kMaxSocketsPerGroup requests should be active.  Let's cancel them.
  ASSERT_LE(kMaxSocketsPerGroup, static_cast<int>(requests()->size()));
  for (int i = 0; i < kMaxSocketsPerGroup; i++)
    (*requests())[i]->handle()->Reset();

  // Let's wait for the rest to complete now.
  for (size_t i = kMaxSocketsPerGroup; i < requests()->size(); ++i) {
    EXPECT_EQ(OK, (*requests())[i]->WaitForResult());
    (*requests())[i]->handle()->Reset();
  }

  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(TransportClientSocketPoolTest, FailingActiveRequestWithPendingRequests) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_FAILING_CLIENT_SOCKET);

  const int kNumRequests = 2 * kMaxSocketsPerGroup + 1;
  ASSERT_LE(kNumRequests, kMaxSockets);  // Otherwise the test will hang.

  // Queue up all the requests
  for (int i = 0; i < kNumRequests; i++)
    EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  for (int i = 0; i < kNumRequests; i++)
    EXPECT_EQ(ERR_CONNECTION_FAILED, (*requests())[i]->WaitForResult());
}

TEST_F(TransportClientSocketPoolTest, IdleSocketLoadTiming) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoConnectedNotReused(handle);

  handle.Reset();
  // Need to run all pending to release the socket back to the pool.
  base::MessageLoop::current()->RunUntilIdle();

  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_.IdleSocketCount());

  rv = handle.Init("a", params_, LOW, callback.callback(), &pool_,
                   BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(0, pool_.IdleSocketCount());
  TestLoadTimingInfoConnectedReused(handle);
}

TEST_F(TransportClientSocketPoolTest, ResetIdleSocketsOnIPAddressChange) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();

  // Need to run all pending to release the socket back to the pool.
  base::MessageLoop::current()->RunUntilIdle();

  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_.IdleSocketCount());

  // After an IP address change, we should have 0 idle sockets.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::MessageLoop::current()->RunUntilIdle();  // Notification happens async.

  EXPECT_EQ(0, pool_.IdleSocketCount());
}

TEST_F(TransportClientSocketPoolTest, BackupSocketConnect) {
  // Case 1 tests the first socket stalling, and the backup connecting.
  MockClientSocketFactory::ClientSocketType case1_types[] = {
    // The first socket will not connect.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET,
    // The second socket will connect more quickly.
    MockClientSocketFactory::MOCK_CLIENT_SOCKET
  };

  // Case 2 tests the first socket being slow, so that we start the
  // second connect, but the second connect stalls, and we still
  // complete the first.
  MockClientSocketFactory::ClientSocketType case2_types[] = {
    // The first socket will connect, although delayed.
    MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
    // The second socket will not connect.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET
  };

  MockClientSocketFactory::ClientSocketType* cases[2] = {
    case1_types,
    case2_types
  };

  for (size_t index = 0; index < arraysize(cases); ++index) {
    client_socket_factory_.set_client_socket_types(cases[index], 2);

    EXPECT_EQ(0, pool_.IdleSocketCount());

    TestCompletionCallback callback;
    ClientSocketHandle handle;
    int rv = handle.Init("b", params_, LOW, callback.callback(), &pool_,
                         BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // Create the first socket, set the timer.
    base::MessageLoop::current()->RunUntilIdle();

    // Wait for the backup socket timer to fire.
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
        ClientSocketPool::kMaxConnectRetryIntervalMs + 50));

    // Let the appropriate socket connect.
    base::MessageLoop::current()->RunUntilIdle();

    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_TRUE(handle.is_initialized());
    EXPECT_TRUE(handle.socket());

    // One socket is stalled, the other is active.
    EXPECT_EQ(0, pool_.IdleSocketCount());
    handle.Reset();

    // Close all pending connect jobs and existing sockets.
    pool_.FlushWithError(ERR_NETWORK_CHANGED);
  }
}

// Test the case where a socket took long enough to start the creation
// of the backup socket, but then we cancelled the request after that.
TEST_F(TransportClientSocketPoolTest, BackupSocketCancel) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET);

  enum { CANCEL_BEFORE_WAIT, CANCEL_AFTER_WAIT };

  for (int index = CANCEL_BEFORE_WAIT; index < CANCEL_AFTER_WAIT; ++index) {
    EXPECT_EQ(0, pool_.IdleSocketCount());

    TestCompletionCallback callback;
    ClientSocketHandle handle;
    int rv = handle.Init("c", params_, LOW, callback.callback(), &pool_,
                         BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // Create the first socket, set the timer.
    base::MessageLoop::current()->RunUntilIdle();

    if (index == CANCEL_AFTER_WAIT) {
      // Wait for the backup socket timer to fire.
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
          ClientSocketPool::kMaxConnectRetryIntervalMs));
    }

    // Let the appropriate socket connect.
    base::MessageLoop::current()->RunUntilIdle();

    handle.Reset();

    EXPECT_FALSE(callback.have_result());
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // One socket is stalled, the other is active.
    EXPECT_EQ(0, pool_.IdleSocketCount());
  }
}

// Test the case where a socket took long enough to start the creation
// of the backup socket and never completes, and then the backup
// connection fails.
TEST_F(TransportClientSocketPoolTest, BackupSocketFailAfterStall) {
  MockClientSocketFactory::ClientSocketType case_types[] = {
    // The first socket will not connect.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET,
    // The second socket will fail immediately.
    MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);

  EXPECT_EQ(0, pool_.IdleSocketCount());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("b", params_, LOW, callback.callback(), &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Create the first socket, set the timer.
  base::MessageLoop::current()->RunUntilIdle();

  // Wait for the backup socket timer to fire.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      ClientSocketPool::kMaxConnectRetryIntervalMs));

  // Let the second connect be synchronous. Otherwise, the emulated
  // host resolution takes an extra trip through the message loop.
  host_resolver_->set_synchronous_mode(true);

  // Let the appropriate socket connect.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(0, pool_.IdleSocketCount());
  handle.Reset();

  // Reset for the next case.
  host_resolver_->set_synchronous_mode(false);
}

// Test the case where a socket took long enough to start the creation
// of the backup socket and eventually completes, but the backup socket
// fails.
TEST_F(TransportClientSocketPoolTest, BackupSocketFailAfterDelay) {
  MockClientSocketFactory::ClientSocketType case_types[] = {
    // The first socket will connect, although delayed.
    MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
    // The second socket will not connect.
    MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);
  client_socket_factory_.set_delay(base::TimeDelta::FromSeconds(5));

  EXPECT_EQ(0, pool_.IdleSocketCount());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("b", params_, LOW, callback.callback(), &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Create the first socket, set the timer.
  base::MessageLoop::current()->RunUntilIdle();

  // Wait for the backup socket timer to fire.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      ClientSocketPool::kMaxConnectRetryIntervalMs));

  // Let the second connect be synchronous. Otherwise, the emulated
  // host resolution takes an extra trip through the message loop.
  host_resolver_->set_synchronous_mode(true);

  // Let the appropriate socket connect.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  handle.Reset();

  // Reset for the next case.
  host_resolver_->set_synchronous_mode(false);
}

// Test the case of the IPv6 address stalling, and falling back to the IPv4
// socket which finishes first.
TEST_F(TransportClientSocketPoolTest, IPv6FallbackSocketIPv4FinishesFirst) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  MockClientSocketFactory::ClientSocketType case_types[] = {
    // This is the IPv6 socket.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET,
    // This is the IPv4 socket.
    MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_->rules()
      ->AddIPLiteralRule("*", "2:abcd::3:4:ff,2.2.2.2", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv4AddressSize, endpoint.address().size());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// Test the case of the IPv6 address being slow, thus falling back to trying to
// connect to the IPv4 address, but having the connect to the IPv6 address
// finish first.
TEST_F(TransportClientSocketPoolTest, IPv6FallbackSocketIPv6FinishesFirst) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  MockClientSocketFactory::ClientSocketType case_types[] = {
    // This is the IPv6 socket.
    MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
    // This is the IPv4 socket.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);
  client_socket_factory_.set_delay(base::TimeDelta::FromMilliseconds(
      TransportConnectJob::kIPv6FallbackTimerInMs + 50));

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_->rules()
      ->AddIPLiteralRule("*", "2:abcd::3:4:ff,2.2.2.2", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv6AddressSize, endpoint.address().size());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

TEST_F(TransportClientSocketPoolTest, IPv6NoIPv4AddressesToFallbackTo) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET);

  // Resolve an AddressList with only IPv6 addresses.
  host_resolver_->rules()
      ->AddIPLiteralRule("*", "2:abcd::3:4:ff,3:abcd::3:4:ff", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv6AddressSize, endpoint.address().size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(TransportClientSocketPoolTest, IPv4HasNoFallback) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET);

  // Resolve an AddressList with only IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule("*", "1.1.1.1", std::string());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", params_, LOW, callback.callback(), &pool,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv4AddressSize, endpoint.address().size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

}  // namespace

}  // namespace net
