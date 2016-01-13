// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_packet_maker.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace {

// This is the expected return from a current server advertising QUIC.
static const char kQuicAlternateProtocolHttpHeader[] =
    "Alternate-Protocol: 80:quic\r\n\r\n";
static const char kQuicAlternateProtocolHttpsHeader[] =
    "Alternate-Protocol: 443:quic\r\n\r\n";

}  // namespace

namespace net {
namespace test {

// Helper class to encapsulate MockReads and MockWrites for QUIC.
// Simplify ownership issues and the interaction with the MockSocketFactory.
class MockQuicData {
 public:
  ~MockQuicData() {
    STLDeleteElements(&packets_);
  }

  void AddRead(scoped_ptr<QuicEncryptedPacket> packet) {
    reads_.push_back(MockRead(SYNCHRONOUS, packet->data(), packet->length(),
                              sequence_number_++));
    packets_.push_back(packet.release());
  }

  void AddRead(IoMode mode, int rv) {
    reads_.push_back(MockRead(mode, rv));
  }

  void AddWrite(scoped_ptr<QuicEncryptedPacket> packet) {
    writes_.push_back(MockWrite(SYNCHRONOUS, packet->data(), packet->length(),
                                sequence_number_++));
    packets_.push_back(packet.release());
  }

  void AddDelayedSocketDataToFactory(MockClientSocketFactory* factory,
                                     size_t delay) {
    MockRead* reads = reads_.empty() ? NULL  : &reads_[0];
    MockWrite* writes = writes_.empty() ? NULL  : &writes_[0];
    socket_data_.reset(new DelayedSocketData(
        delay, reads, reads_.size(), writes, writes_.size()));
    factory->AddSocketDataProvider(socket_data_.get());
  }

 private:
  std::vector<QuicEncryptedPacket*> packets_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  size_t sequence_number_;
  scoped_ptr<SocketDataProvider> socket_data_;
};

class QuicNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<QuicVersion> {
 protected:
  QuicNetworkTransactionTest()
      : maker_(GetParam(), 0),
        clock_(new MockClock),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_service_(ProxyService::CreateDirect()),
        auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        random_generator_(0),
        hanging_data_(NULL, 0, NULL, 0) {
    request_.method = "GET";
    request_.url = GURL("http://www.google.com/");
    request_.load_flags = 0;
    clock_->AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  }

  virtual void SetUp() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  virtual void TearDown() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  scoped_ptr<QuicEncryptedPacket> ConstructConnectionClosePacket(
      QuicPacketSequenceNumber num) {
    return maker_.MakeConnectionClosePacket(num);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    return maker_.MakeAckPacket(2, largest_received, least_unacked, true);
  }

  SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                    const std::string& scheme,
                                    const std::string& path) {
    return maker_.GetRequestHeaders(method, scheme, path);
  }

  SpdyHeaderBlock GetResponseHeaders(const std::string& status) {
    return maker_.GetResponseHeaders(status);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    return maker_.MakeDataPacket(
        sequence_number, stream_id, should_include_version, fin, offset, data);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers) {
    return maker_.MakeRequestHeadersPacket(
        sequence_number, stream_id, should_include_version, fin, headers);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers) {
    return maker_.MakeResponseHeadersPacket(
        sequence_number, stream_id, should_include_version, fin, headers);
  }

  void CreateSession() {
    CreateSessionWithFactory(&socket_factory_, false);
  }

  void CreateSessionWithNextProtos() {
    CreateSessionWithFactory(&socket_factory_, true);
  }

  // If |use_next_protos| is true, enables SPDY and QUIC.
  void CreateSessionWithFactory(ClientSocketFactory* socket_factory,
                                bool use_next_protos) {
    params_.enable_quic = true;
    params_.quic_clock = clock_;
    params_.quic_random = &random_generator_;
    params_.client_socket_factory = socket_factory;
    params_.quic_crypto_client_stream_factory = &crypto_client_stream_factory_;
    params_.host_resolver = &host_resolver_;
    params_.cert_verifier = &cert_verifier_;
    params_.transport_security_state = &transport_security_state_;
    params_.proxy_service = proxy_service_.get();
    params_.ssl_config_service = ssl_config_service_.get();
    params_.http_auth_handler_factory = auth_handler_factory_.get();
    params_.http_server_properties = http_server_properties.GetWeakPtr();
    params_.quic_supported_versions = SupportedVersions(GetParam());

    if (use_next_protos) {
      params_.use_alternate_protocols = true;
      params_.next_protos = NextProtosSpdy3();
    }

    session_ = new HttpNetworkSession(params_);
    session_->quic_stream_factory()->set_require_confirmation(false);
  }

  void CheckWasQuicResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    ASSERT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3,
              response->connection_info);
  }

  void CheckWasHttpResponse(const scoped_ptr<HttpNetworkTransaction>& trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    ASSERT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_FALSE(response->was_npn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP1,
              response->connection_info);
  }

  void CheckResponseData(HttpNetworkTransaction* trans,
                         const std::string& expected) {
    std::string response_data;
    ASSERT_EQ(OK, ReadTransaction(trans, &response_data));
    EXPECT_EQ(expected, response_data);
  }

  void RunTransaction(HttpNetworkTransaction* trans) {
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, callback.WaitForResult());
  }

  void SendRequestAndExpectHttpResponse(const std::string& expected) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    RunTransaction(trans.get());
    CheckWasHttpResponse(trans);
    CheckResponseData(trans.get(), expected);
  }

  void SendRequestAndExpectQuicResponse(const std::string& expected) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
    RunTransaction(trans.get());
    CheckWasQuicResponse(trans);
    CheckResponseData(trans.get(), expected);
  }

  void AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    session_->http_server_properties()->SetAlternateProtocol(
        HostPortPair::FromURL(request_.url), 80, QUIC);
  }

  void ExpectBrokenAlternateProtocolMapping() {
    ASSERT_TRUE(session_->http_server_properties()->HasAlternateProtocol(
        HostPortPair::FromURL(request_.url)));
    const PortAlternateProtocolPair alternate =
        session_->http_server_properties()->GetAlternateProtocol(
            HostPortPair::FromURL(request_.url));
    EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol);
  }

  void ExpectQuicAlternateProtocolMapping() {
    ASSERT_TRUE(session_->http_server_properties()->HasAlternateProtocol(
        HostPortPair::FromURL(request_.url)));
    const PortAlternateProtocolPair alternate =
        session_->http_server_properties()->GetAlternateProtocol(
            HostPortPair::FromURL(request_.url));
    EXPECT_EQ(QUIC, alternate.protocol);
  }

  void AddHangingNonAlternateProtocolSocketData() {
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data_.set_connect_data(hanging_connect);
    socket_factory_.AddSocketDataProvider(&hanging_data_);
  }

  QuicTestPacketMaker maker_;
  scoped_refptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockClock* clock_;  // Owned by QuicStreamFactory after CreateSession.
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  MockRandom random_generator_;
  HttpServerPropertiesImpl http_server_properties;
  HttpNetworkSession::Params params_;
  HttpRequestInfo request_;
  CapturingBoundNetLog net_log_;
  StaticSocketDataProvider hanging_data_;
};

INSTANTIATE_TEST_CASE_P(Version, QuicNetworkTransactionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicNetworkTransactionTest, ForceQuic) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:80");

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED.
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  std::string packet_sequence_number;
  ASSERT_TRUE(entries[pos].GetStringValue(
      "packet_sequence_number", &packet_sequence_number));
  EXPECT_EQ("1", packet_sequence_number);

  // ... and also a QUIC_SESSION_STREAM_FRAME_RECEIVED.
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_STREAM_FRAME_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  int log_stream_id;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &log_stream_id));
  EXPECT_EQ(3, log_stream_id);
}

TEST_P(QuicNetworkTransactionTest, QuicProxy) {
  proxy_service_.reset(
      ProxyService::CreateFixedFromPacResult("QUIC myproxy:70"));

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // There is no need to set up an alternate protocol job, because
  // no attempt will be made to speak to the proxy over TCP.

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ForceQuicWithErrorConnecting) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:80");

  MockQuicData mock_quic_data;
  mock_quic_data.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);

  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 0);

  CreateSession();

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
}

TEST_P(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider data(http_reads, arraysize(http_reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolForQuic) {
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, UseAlternateProtocolForQuicForHttps) {
  params_.origin_to_force_quic_on =
      HostPortPair::FromString("www.google.com:443");
  params_.enable_quic_https = true;

  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpsHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, HungAlternateProtocol) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  MockWrite http_writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
    MockWrite(SYNCHRONOUS, 1, "Host: www.google.com\r\n"),
    MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")
  };

  MockRead http_reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHttpHeader),
    MockRead(SYNCHRONOUS, 5, "hello world"),
    MockRead(SYNCHRONOUS, OK, 6)
  };

  DeterministicMockClientSocketFactory socket_factory;

  DeterministicSocketData http_data(http_reads, arraysize(http_reads),
                                    http_writes, arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data);

  // The QUIC transaction will not be allowed to complete.
  MockWrite quic_writes[] = {
    MockWrite(ASYNC, ERR_IO_PENDING, 0)
  };
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_IO_PENDING, 1),
  };
  DeterministicSocketData quic_data(quic_reads, arraysize(quic_reads),
                                    quic_writes, arraysize(quic_writes));
  socket_factory.AddSocketDataProvider(&quic_data);

  // The HTTP transaction will complete.
  DeterministicSocketData http_data2(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  socket_factory.AddSocketDataProvider(&http_data2);

  CreateSessionWithFactory(&socket_factory, true);

  // Run the first request.
  http_data.StopAfter(arraysize(http_reads) + arraysize(http_writes));
  SendRequestAndExpectHttpResponse("hello world");
  ASSERT_TRUE(http_data.at_read_eof());
  ASSERT_TRUE(http_data.at_write_eof());

  // Now run the second request in which the QUIC socket hangs,
  // and verify the the transaction continues over HTTP.
  http_data2.StopAfter(arraysize(http_reads) + arraysize(http_writes));
  SendRequestAndExpectHttpResponse("hello world");

  ASSERT_TRUE(http_data2.at_read_eof());
  ASSERT_TRUE(http_data2.at_write_eof());
  ASSERT_TRUE(!quic_data.at_read_eof());
  ASSERT_TRUE(!quic_data.at_write_eof());
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithHttpRace) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithNoHttpRace) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF
  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         NULL,
                         net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithProxy) {
  proxy_service_.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));

  // Since we are using a proxy, the QUIC job will not succeed.
  MockWrite http_writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET http://www.google.com/ HTTP/1.1\r\n"),
    MockWrite(SYNCHRONOUS, 1, "Host: www.google.com\r\n"),
    MockWrite(SYNCHRONOUS, 2, "Proxy-Connection: keep-alive\r\n\r\n")
  };

  MockRead http_reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 4, kQuicAlternateProtocolHttpHeader),
    MockRead(SYNCHRONOUS, 5, "hello world"),
    MockRead(SYNCHRONOUS, OK, 6)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         NULL,
                         net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithConfirmationRequired) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ConstructResponseHeadersPacket(1, kClientDataStreamId1, false, false,
                                     GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ConstructDataPacket(2, kClientDataStreamId1, false, true, 0, "hello!"));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF
  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 1);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info, DEFAULT_PRIORITY, &address,
                         CompletionCallback(), NULL, net_log_.bound());

  CreateSessionWithNextProtos();
  session_->quic_stream_factory()->set_require_confirmation(true);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  crypto_client_stream_factory_.last_stream()->SendOnCryptoHandshakeEvent(
      QuicSession::HANDSHAKE_CONFIRMED);
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocol) {
  // Alternate-protocol job
  scoped_ptr<QuicEncryptedPacket> close(ConstructConnectionClosePacket(1));
  MockRead quic_reads[] = {
    MockRead(ASYNC, close->data(), close->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolReadError) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, NoBrokenAlternateProtocolIfTcpFails) {
  // Alternate-protocol job will fail when the session attempts to read.
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job will also fail.
  MockRead http_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, callback.WaitForResult());
  ExpectQuicAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, FailedZeroRttBrokenAlternateProtocol) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  AddHangingNonAlternateProtocolSocketData();

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, arraysize(quic_reads),
                                      NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();

  EXPECT_TRUE(quic_data.at_read_eof());
  EXPECT_TRUE(quic_data.at_write_eof());
}

TEST_P(QuicNetworkTransactionTest, DISABLED_HangingZeroRttFallback) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
    MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider quic_data(quic_reads, arraysize(quic_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolOnConnectFailure) {
  // Alternate-protocol job will fail before creating a QUIC session.
  StaticSocketDataProvider quic_data(NULL, 0, NULL, 0);
  quic_data.set_connect_data(MockConnect(SYNCHRONOUS,
                                         ERR_INTERNET_DISCONNECTED));
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnect) {
  MockQuicData mock_quic_data;
  mock_quic_data.AddRead(ConstructConnectionClosePacket(1));
  mock_quic_data.AddWrite(
      ConstructRequestHeadersPacket(1, kClientDataStreamId1, true, true,
                                    GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddWrite(ConstructAckPacket(2, 1));
  mock_quic_data.AddDelayedSocketDataToFactory(&socket_factory_, 0);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     NULL, 0);
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("www.google.com", "192.168.0.1", "");
  HostResolver::RequestInfo info(HostPortPair("www.google.com", 80));
  AddressList address;
  host_resolver_.Resolve(info,
                         DEFAULT_PRIORITY,
                         &address,
                         CompletionCallback(),
                         NULL,
                         net_log_.bound());

  CreateSessionWithNextProtos();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

}  // namespace test
}  // namespace net
