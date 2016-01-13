// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/rand_util.h"
#include "base/sys_byteorder.h"
#include "base/test/test_timeouts.h"
#include "net/base/dns_util.h"
#include "net/base/net_log.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string DomainFromDot(const base::StringPiece& dotted) {
  std::string out;
  EXPECT_TRUE(DNSDomainFromDot(dotted, &out));
  return out;
}

// A SocketDataProvider builder.
class DnsSocketData {
 public:
  // The ctor takes parameters for the DnsQuery.
  DnsSocketData(uint16 id,
                const char* dotted_name,
                uint16 qtype,
                IoMode mode,
                bool use_tcp)
      : query_(new DnsQuery(id, DomainFromDot(dotted_name), qtype)),
        use_tcp_(use_tcp) {
    if (use_tcp_) {
      scoped_ptr<uint16> length(new uint16);
      *length = base::HostToNet16(query_->io_buffer()->size());
      writes_.push_back(MockWrite(mode,
                                  reinterpret_cast<const char*>(length.get()),
                                  sizeof(uint16)));
      lengths_.push_back(length.release());
    }
    writes_.push_back(MockWrite(mode,
                                query_->io_buffer()->data(),
                                query_->io_buffer()->size()));
  }
  ~DnsSocketData() {}

  // All responses must be added before GetProvider.

  // Adds pre-built DnsResponse. |tcp_length| will be used in TCP mode only.
  void AddResponseWithLength(scoped_ptr<DnsResponse> response, IoMode mode,
                             uint16 tcp_length) {
    CHECK(!provider_.get());
    if (use_tcp_) {
      scoped_ptr<uint16> length(new uint16);
      *length = base::HostToNet16(tcp_length);
      reads_.push_back(MockRead(mode,
                                reinterpret_cast<const char*>(length.get()),
                                sizeof(uint16)));
      lengths_.push_back(length.release());
    }
    reads_.push_back(MockRead(mode,
                              response->io_buffer()->data(),
                              response->io_buffer()->size()));
    responses_.push_back(response.release());
  }

  // Adds pre-built DnsResponse.
  void AddResponse(scoped_ptr<DnsResponse> response, IoMode mode) {
    uint16 tcp_length = response->io_buffer()->size();
    AddResponseWithLength(response.Pass(), mode, tcp_length);
  }

  // Adds pre-built response from |data| buffer.
  void AddResponseData(const uint8* data, size_t length, IoMode mode) {
    CHECK(!provider_.get());
    AddResponse(make_scoped_ptr(
        new DnsResponse(reinterpret_cast<const char*>(data), length, 0)), mode);
  }

  // Add no-answer (RCODE only) response matching the query.
  void AddRcode(int rcode, IoMode mode) {
    scoped_ptr<DnsResponse> response(
        new DnsResponse(query_->io_buffer()->data(),
                        query_->io_buffer()->size(),
                        0));
    dns_protocol::Header* header =
        reinterpret_cast<dns_protocol::Header*>(response->io_buffer()->data());
    header->flags |= base::HostToNet16(dns_protocol::kFlagResponse | rcode);
    AddResponse(response.Pass(), mode);
  }

  // Build, if needed, and return the SocketDataProvider. No new responses
  // should be added afterwards.
  SocketDataProvider* GetProvider() {
    if (provider_.get())
      return provider_.get();
    // Terminate the reads with ERR_IO_PENDING to prevent overrun and default to
    // timeout.
    reads_.push_back(MockRead(ASYNC, ERR_IO_PENDING));
    provider_.reset(new DelayedSocketData(1, &reads_[0], reads_.size(),
                                          &writes_[0], writes_.size()));
    if (use_tcp_) {
      provider_->set_connect_data(MockConnect(reads_[0].mode, OK));
    }
    return provider_.get();
  }

  uint16 query_id() const {
    return query_->id();
  }

  // Returns true if the expected query was written to the socket.
  bool was_written() const {
    CHECK(provider_.get());
    return provider_->write_index() > 0;
  }

 private:
  scoped_ptr<DnsQuery> query_;
  bool use_tcp_;
  ScopedVector<uint16> lengths_;
  ScopedVector<DnsResponse> responses_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  scoped_ptr<DelayedSocketData> provider_;

  DISALLOW_COPY_AND_ASSIGN(DnsSocketData);
};

class TestSocketFactory;

// A variant of MockUDPClientSocket which always fails to Connect.
class FailingUDPClientSocket : public MockUDPClientSocket {
 public:
  FailingUDPClientSocket(SocketDataProvider* data,
                         net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log) {
  }
  virtual ~FailingUDPClientSocket() {}
  virtual int Connect(const IPEndPoint& endpoint) OVERRIDE {
    return ERR_CONNECTION_REFUSED;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingUDPClientSocket);
};

// A variant of MockUDPClientSocket which notifies the factory OnConnect.
class TestUDPClientSocket : public MockUDPClientSocket {
 public:
  TestUDPClientSocket(TestSocketFactory* factory,
                      SocketDataProvider* data,
                      net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log), factory_(factory) {
  }
  virtual ~TestUDPClientSocket() {}
  virtual int Connect(const IPEndPoint& endpoint) OVERRIDE;

 private:
  TestSocketFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(TestUDPClientSocket);
};

// Creates TestUDPClientSockets and keeps endpoints reported via OnConnect.
class TestSocketFactory : public MockClientSocketFactory {
 public:
  TestSocketFactory() : fail_next_socket_(false) {}
  virtual ~TestSocketFactory() {}

  virtual scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      net::NetLog* net_log,
      const net::NetLog::Source& source) OVERRIDE {
    if (fail_next_socket_) {
      fail_next_socket_ = false;
      return scoped_ptr<DatagramClientSocket>(
          new FailingUDPClientSocket(&empty_data_, net_log));
    }
    SocketDataProvider* data_provider = mock_data().GetNext();
    scoped_ptr<TestUDPClientSocket> socket(
        new TestUDPClientSocket(this, data_provider, net_log));
    data_provider->set_socket(socket.get());
    return socket.PassAs<DatagramClientSocket>();
  }

  void OnConnect(const IPEndPoint& endpoint) {
    remote_endpoints_.push_back(endpoint);
  }

  std::vector<IPEndPoint> remote_endpoints_;
  bool fail_next_socket_;

 private:
  StaticSocketDataProvider empty_data_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketFactory);
};

int TestUDPClientSocket::Connect(const IPEndPoint& endpoint) {
  factory_->OnConnect(endpoint);
  return MockUDPClientSocket::Connect(endpoint);
}

// Helper class that holds a DnsTransaction and handles OnTransactionComplete.
class TransactionHelper {
 public:
  // If |expected_answer_count| < 0 then it is the expected net error.
  TransactionHelper(const char* hostname,
                    uint16 qtype,
                    int expected_answer_count)
      : hostname_(hostname),
        qtype_(qtype),
        expected_answer_count_(expected_answer_count),
        cancel_in_callback_(false),
        quit_in_callback_(false),
        completed_(false) {
  }

  // Mark that the transaction shall be destroyed immediately upon callback.
  void set_cancel_in_callback() {
    cancel_in_callback_ = true;
  }

  // Mark to call MessageLoop::Quit() upon callback.
  void set_quit_in_callback() {
    quit_in_callback_ = true;
  }

  void StartTransaction(DnsTransactionFactory* factory) {
    EXPECT_EQ(NULL, transaction_.get());
    transaction_ = factory->CreateTransaction(
        hostname_,
        qtype_,
        base::Bind(&TransactionHelper::OnTransactionComplete,
                   base::Unretained(this)),
        BoundNetLog());
    EXPECT_EQ(hostname_, transaction_->GetHostname());
    EXPECT_EQ(qtype_, transaction_->GetType());
    transaction_->Start();
  }

  void Cancel() {
    ASSERT_TRUE(transaction_.get() != NULL);
    transaction_.reset(NULL);
  }

  void OnTransactionComplete(DnsTransaction* t,
                             int rv,
                             const DnsResponse* response) {
    EXPECT_FALSE(completed_);
    EXPECT_EQ(transaction_.get(), t);

    completed_ = true;

    if (cancel_in_callback_) {
      Cancel();
      return;
    }

    // Tell MessageLoop to quit now, in case any ASSERT_* fails.
    if (quit_in_callback_)
      base::MessageLoop::current()->Quit();

    if (expected_answer_count_ >= 0) {
      ASSERT_EQ(OK, rv);
      ASSERT_TRUE(response != NULL);
      EXPECT_EQ(static_cast<unsigned>(expected_answer_count_),
                response->answer_count());
      EXPECT_EQ(qtype_, response->qtype());

      DnsRecordParser parser = response->Parser();
      DnsResourceRecord record;
      for (int i = 0; i < expected_answer_count_; ++i) {
        EXPECT_TRUE(parser.ReadRecord(&record));
      }
    } else {
      EXPECT_EQ(expected_answer_count_, rv);
    }
  }

  bool has_completed() const {
    return completed_;
  }

  // Shorthands for commonly used commands.

  bool Run(DnsTransactionFactory* factory) {
    StartTransaction(factory);
    base::MessageLoop::current()->RunUntilIdle();
    return has_completed();
  }

  // Use when some of the responses are timeouts.
  bool RunUntilDone(DnsTransactionFactory* factory) {
    set_quit_in_callback();
    StartTransaction(factory);
    base::MessageLoop::current()->Run();
    return has_completed();
  }

 private:
  std::string hostname_;
  uint16 qtype_;
  scoped_ptr<DnsTransaction> transaction_;
  int expected_answer_count_;
  bool cancel_in_callback_;
  bool quit_in_callback_;

  bool completed_;
};

class DnsTransactionTest : public testing::Test {
 public:
  DnsTransactionTest() {}

  // Generates |nameservers| for DnsConfig.
  void ConfigureNumServers(unsigned num_servers) {
    CHECK_LE(num_servers, 255u);
    config_.nameservers.clear();
    IPAddressNumber dns_ip;
    {
      bool rv = ParseIPLiteralToNumber("192.168.1.0", &dns_ip);
      EXPECT_TRUE(rv);
    }
    for (unsigned i = 0; i < num_servers; ++i) {
      dns_ip[3] = i;
      config_.nameservers.push_back(IPEndPoint(dns_ip,
                                               dns_protocol::kDefaultPort));
    }
  }

  // Called after fully configuring |config|.
  void ConfigureFactory() {
    socket_factory_.reset(new TestSocketFactory());
    session_ = new DnsSession(
        config_,
        DnsSocketPool::CreateNull(socket_factory_.get()),
        base::Bind(&DnsTransactionTest::GetNextId, base::Unretained(this)),
        NULL /* NetLog */);
    transaction_factory_ = DnsTransactionFactory::CreateFactory(session_.get());
  }

  void AddSocketData(scoped_ptr<DnsSocketData> data) {
    CHECK(socket_factory_.get());
    transaction_ids_.push_back(data->query_id());
    socket_factory_->AddSocketDataProvider(data->GetProvider());
    socket_data_.push_back(data.release());
  }

  // Add expected query for |dotted_name| and |qtype| with |id| and response
  // taken verbatim from |data| of |data_length| bytes. The transaction id in
  // |data| should equal |id|, unless testing mismatched response.
  void AddQueryAndResponse(uint16 id,
                           const char* dotted_name,
                           uint16 qtype,
                           const uint8* response_data,
                           size_t response_length,
                           IoMode mode,
                           bool use_tcp) {
    CHECK(socket_factory_.get());
    scoped_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, mode, use_tcp));
    data->AddResponseData(response_data, response_length, mode);
    AddSocketData(data.Pass());
  }

  void AddAsyncQueryAndResponse(uint16 id,
                                const char* dotted_name,
                                uint16 qtype,
                                const uint8* data,
                                size_t data_length) {
    AddQueryAndResponse(id, dotted_name, qtype, data, data_length, ASYNC,
                        false);
  }

  void AddSyncQueryAndResponse(uint16 id,
                               const char* dotted_name,
                               uint16 qtype,
                               const uint8* data,
                               size_t data_length) {
    AddQueryAndResponse(id, dotted_name, qtype, data, data_length, SYNCHRONOUS,
                        false);
  }

  // Add expected query of |dotted_name| and |qtype| and no response.
  void AddQueryAndTimeout(const char* dotted_name, uint16 qtype) {
    uint16 id = base::RandInt(0, kuint16max);
    scoped_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, ASYNC, false));
    AddSocketData(data.Pass());
  }

  // Add expected query of |dotted_name| and |qtype| and matching response with
  // no answer and RCODE set to |rcode|. The id will be generated randomly.
  void AddQueryAndRcode(const char* dotted_name,
                        uint16 qtype,
                        int rcode,
                        IoMode mode,
                        bool use_tcp) {
    CHECK_NE(dns_protocol::kRcodeNOERROR, rcode);
    uint16 id = base::RandInt(0, kuint16max);
    scoped_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, mode, use_tcp));
    data->AddRcode(rcode, mode);
    AddSocketData(data.Pass());
  }

  void AddAsyncQueryAndRcode(const char* dotted_name, uint16 qtype, int rcode) {
    AddQueryAndRcode(dotted_name, qtype, rcode, ASYNC, false);
  }

  void AddSyncQueryAndRcode(const char* dotted_name, uint16 qtype, int rcode) {
    AddQueryAndRcode(dotted_name, qtype, rcode, SYNCHRONOUS, false);
  }

  // Checks if the sockets were connected in the order matching the indices in
  // |servers|.
  void CheckServerOrder(const unsigned* servers, size_t num_attempts) {
    ASSERT_EQ(num_attempts, socket_factory_->remote_endpoints_.size());
    for (size_t i = 0; i < num_attempts; ++i) {
      EXPECT_EQ(socket_factory_->remote_endpoints_[i],
                session_->config().nameservers[servers[i]]);
    }
  }

  virtual void SetUp() OVERRIDE {
    // By default set one server,
    ConfigureNumServers(1);
    // and no retransmissions,
    config_.attempts = 1;
    // but long enough timeout for memory tests.
    config_.timeout = TestTimeouts::action_timeout();
    ConfigureFactory();
  }

  virtual void TearDown() OVERRIDE {
    // Check that all socket data was at least written to.
    for (size_t i = 0; i < socket_data_.size(); ++i) {
      EXPECT_TRUE(socket_data_[i]->was_written()) << i;
    }
  }

 protected:
  int GetNextId(int min, int max) {
    EXPECT_FALSE(transaction_ids_.empty());
    int id = transaction_ids_.front();
    transaction_ids_.pop_front();
    EXPECT_GE(id, min);
    EXPECT_LE(id, max);
    return id;
  }

  DnsConfig config_;

  ScopedVector<DnsSocketData> socket_data_;

  std::deque<int> transaction_ids_;
  scoped_ptr<TestSocketFactory> socket_factory_;
  scoped_refptr<DnsSession> session_;
  scoped_ptr<DnsTransactionFactory> transaction_factory_;
};

TEST_F(DnsTransactionTest, Lookup) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

// Concurrent lookup tests assume that DnsTransaction::Start immediately
// consumes a socket from ClientSocketFactory.
TEST_F(DnsTransactionTest, ConcurrentLookup) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  AddAsyncQueryAndResponse(1 /* id */, kT1HostName, kT1Qtype,
                           kT1ResponseDatagram, arraysize(kT1ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());
  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get());

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, CancelLookup) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  AddAsyncQueryAndResponse(1 /* id */, kT1HostName, kT1Qtype,
                           kT1ResponseDatagram, arraysize(kT1ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());
  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get());

  helper0.Cancel();

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DestroyFactory) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());

  // Destroying the client does not affect running requests.
  transaction_factory_.reset(NULL);

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTest, CancelFromCallback) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.set_cancel_in_callback();
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseSync) {
  config_.attempts = 2;
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();

  // Attempt receives mismatched response followed by valid response.
  scoped_ptr<DnsSocketData> data(
      new DnsSocketData(0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, false));
  data->AddResponseData(kT1ResponseDatagram,
                        arraysize(kT1ResponseDatagram), SYNCHRONOUS);
  data->AddResponseData(kT0ResponseDatagram,
                        arraysize(kT0ResponseDatagram), SYNCHRONOUS);
  AddSocketData(data.Pass());

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseAsync) {
  config_.attempts = 2;
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();

  // First attempt receives mismatched response followed by valid response.
  // Second attempt times out.
  scoped_ptr<DnsSocketData> data(
      new DnsSocketData(0 /* id */, kT0HostName, kT0Qtype, ASYNC, false));
  data->AddResponseData(kT1ResponseDatagram,
                        arraysize(kT1ResponseDatagram), ASYNC);
  data->AddResponseData(kT0ResponseDatagram,
                        arraysize(kT0ResponseDatagram), ASYNC);
  AddSocketData(data.Pass());
  AddQueryAndTimeout(kT0HostName, kT0Qtype);

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseFail) {
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();

  // Attempt receives mismatched response but times out because only one attempt
  // is allowed.
  AddAsyncQueryAndResponse(1 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_TIMED_OUT);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ServerFail) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, NoDomain) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, Timeout) {
  config_.attempts = 3;
  // Use short timeout to speed up the test.
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();

  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_TIMED_OUT);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  EXPECT_TRUE(base::MessageLoop::current()->IsIdleForTesting());
}

TEST_F(DnsTransactionTest, ServerFallbackAndRotate) {
  // Test that we fallback on both server failure and timeout.
  config_.attempts = 2;
  // The next request should start from the next server.
  config_.rotate = true;
  ConfigureNumServers(3);
  // Use short timeout to speed up the test.
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();

  // Responses for first request.
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);
  // Responses for second request.
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_NAME_NOT_RESOLVED);
  TransactionHelper helper1(kT1HostName, kT1Qtype, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  unsigned kOrder[] = {
      0, 1, 2, 0, 1,    // The first transaction.
      1, 2, 0,          // The second transaction starts from the next server.
  };
  CheckServerOrder(kOrder, arraysize(kOrder));
}

TEST_F(DnsTransactionTest, SuffixSearchAboveNdots) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  config_.rotate = true;
  ConfigureNumServers(2);
  ConfigureFactory();

  AddAsyncQueryAndRcode("x.y.z", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // Also check if suffix search causes server rotation.
  unsigned kOrder0[] = { 0, 1, 0, 1 };
  CheckServerOrder(kOrder0, arraysize(kOrder0));
}

TEST_F(DnsTransactionTest, SuffixSearchBelowNdots) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  ConfigureFactory();

  // Responses for first transaction.
  AddAsyncQueryAndRcode("x.y.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for second transaction.
  AddAsyncQueryAndRcode("x.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for third transaction.
  AddAsyncQueryAndRcode("x", dns_protocol::kTypeAAAA,
                        dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0("x.y", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // A single-label name.
  TransactionHelper helper1("x", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  // A fully-qualified name.
  TransactionHelper helper2("x.", dns_protocol::kTypeAAAA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper2.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, EmptySuffixSearch) {
  // Responses for first transaction.
  AddAsyncQueryAndRcode("x", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  // A fully-qualified name.
  TransactionHelper helper0("x.", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // A single label name is not even attempted.
  TransactionHelper helper1("singlelabel", dns_protocol::kTypeA,
                            ERR_DNS_SEARCH_EMPTY);

  helper1.Run(transaction_factory_.get());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DontAppendToMultiLabelName) {
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  config_.append_to_multi_label_name = false;
  ConfigureFactory();

  // Responses for first transaction.
  AddAsyncQueryAndRcode("x.y.z", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for second transaction.
  AddAsyncQueryAndRcode("x.y", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for third transaction.
  AddAsyncQueryAndRcode("x.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  TransactionHelper helper1("x.y", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  TransactionHelper helper2("x", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper2.Run(transaction_factory_.get()));
}

const uint8 kResponseNoData[] = {
  0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
  // Question
  0x01,  'x', 0x01,  'y', 0x01,  'z', 0x01,  'b', 0x00, 0x00, 0x01, 0x00, 0x01,
  // Authority section, SOA record, TTL 0x3E6
  0x01,  'z', 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x03, 0xE6,
  // Minimal RDATA, 18 bytes
  0x00, 0x12,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

TEST_F(DnsTransactionTest, SuffixSearchStop) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  ConfigureFactory();

  AddAsyncQueryAndRcode("x.y.z", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndResponse(0 /* id */, "x.y.z.b", dns_protocol::kTypeA,
                           kResponseNoData, arraysize(kResponseNoData));

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA, 0 /* answers */);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncFirstQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddSyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                          kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncFirstQueryWithSearch) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddSyncQueryAndRcode("www.lab.ccs.neu.edu", kT2Qtype,
                       dns_protocol::kRcodeNXDOMAIN);
  // "www.ccs.neu.edu"
  AddAsyncQueryAndResponse(2 /* id */, kT2HostName, kT2Qtype,
                           kT2ResponseDatagram, arraysize(kT2ResponseDatagram));

  TransactionHelper helper0("www", kT2Qtype, kT2RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncSearchQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddAsyncQueryAndRcode("www.lab.ccs.neu.edu", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddSyncQueryAndResponse(2 /* id */, kT2HostName, kT2Qtype,
                          kT2ResponseDatagram, arraysize(kT2ResponseDatagram));

  TransactionHelper helper0("www", kT2Qtype, kT2RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ConnectFailure) {
  socket_factory_->fail_next_socket_ = true;
  transaction_ids_.push_back(0);  // Needed to make a DnsUDPAttempt.
  TransactionHelper helper0("www.chromium.org", dns_protocol::kTypeA,
                            ERR_CONNECTION_REFUSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ConnectFailureFollowedBySuccess) {
  // Retry after server failure.
  config_.attempts = 2;
  ConfigureFactory();
  // First server connection attempt fails.
  transaction_ids_.push_back(0);  // Needed to make a DnsUDPAttempt.
  socket_factory_->fail_next_socket_ = true;
  // Second DNS query succeeds.
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPLookup) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                      kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                      ASYNC, true /* use_tcp */);

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPFailure) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   ASYNC, true /* use_tcp */);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPMalformed) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  scoped_ptr<DnsSocketData> data(
      new DnsSocketData(0 /* id */, kT0HostName, kT0Qtype, ASYNC, true));
  // Valid response but length too short.
  data->AddResponseWithLength(
      make_scoped_ptr(
          new DnsResponse(reinterpret_cast<const char*>(kT0ResponseDatagram),
                          arraysize(kT0ResponseDatagram), 0)),
      ASYNC,
      static_cast<uint16>(kT0QuerySize - 1));
  AddSocketData(data.Pass());

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPTimeout) {
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddSocketData(make_scoped_ptr(
      new DnsSocketData(1 /* id */, kT0HostName, kT0Qtype, ASYNC, true)));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_TIMED_OUT);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, InvalidQuery) {
  config_.timeout = TestTimeouts::tiny_timeout();
  ConfigureFactory();

  TransactionHelper helper0(".", dns_protocol::kTypeA, ERR_INVALID_ARGUMENT);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

}  // namespace

}  // namespace net
