// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_proxy_client_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/address_list.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace {

enum TestCase {
  // Test using the SPDY/3.1 protocol.
  kTestCaseSPDY31,

  // Test using the HTTP/2 protocol, without specifying a stream
  // dependency based on the RequestPriority.
  kTestCaseHTTP2NoPriorityDependencies,

  // Test using the HTTP/2 protocol, specifying a stream
  // dependency based on the RequestPriority.
  kTestCaseHTTP2PriorityDependencies
};

static const char kRequestUrl[] = "https://www.google.com/";
static const char kOriginHost[] = "www.google.com";
static const int kOriginPort = 443;
static const char kOriginHostPort[] = "www.google.com:443";
static const char kProxyUrl[] = "https://myproxy:6121/";
static const char kProxyHost[] = "myproxy";
static const int kProxyPort = 6121;
static const char kUserAgent[] = "Mozilla/1.0";

static const int kStreamId = 1;

static const char kMsg1[] = "\0hello!\xff";
static const int kLen1 = 8;
static const char kMsg2[] = "\0a2345678\0";
static const int kLen2 = 10;
static const char kMsg3[] = "bye!";
static const int kLen3 = 4;
static const char kMsg33[] = "bye!bye!";
static const int kLen33 = kLen3 + kLen3;
static const char kMsg333[] = "bye!bye!bye!";
static const int kLen333 = kLen3 + kLen3 + kLen3;

static const char kRedirectUrl[] = "https://example.com/";

}  // anonymous namespace

namespace net {

class SpdyProxyClientSocketTest : public PlatformTest,
                                  public testing::WithParamInterface<TestCase> {
 public:
  SpdyProxyClientSocketTest();
  ~SpdyProxyClientSocketTest();

  void TearDown() override;

 protected:
  NextProto GetProtocol() const;
  bool GetDependenciesFromPriority() const;

  void Initialize(MockRead* reads, size_t reads_count, MockWrite* writes,
                  size_t writes_count);
  void PopulateConnectRequestIR(SpdyHeaderBlock* syn_ir);
  void PopulateConnectReplyIR(SpdyHeaderBlock* block, const char* status);
  SpdySerializedFrame* ConstructConnectRequestFrame();
  SpdySerializedFrame* ConstructConnectAuthRequestFrame();
  SpdySerializedFrame* ConstructConnectReplyFrame();
  SpdySerializedFrame* ConstructConnectAuthReplyFrame();
  SpdySerializedFrame* ConstructConnectRedirectReplyFrame();
  SpdySerializedFrame* ConstructConnectErrorReplyFrame();
  SpdySerializedFrame* ConstructBodyFrame(const char* data, int length);
  scoped_refptr<IOBufferWithSize> CreateBuffer(const char* data, int size);
  void AssertConnectSucceeds();
  void AssertConnectFails(int result);
  void AssertConnectionEstablished();
  void AssertSyncReadEquals(const char* data, int len);
  void AssertAsyncReadEquals(const char* data, int len);
  void AssertReadStarts(const char* data, int len);
  void AssertReadReturns(const char* data, int len);
  void AssertAsyncWriteSucceeds(const char* data, int len);
  void AssertWriteReturns(const char* data, int len, int rv);
  void AssertWriteLength(int len);

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL(kProxyUrl),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  void ResumeAndRun() {
    // Run until the pause, if the provider isn't paused yet.
    data_->RunUntilPaused();
    data_->Resume();
    base::RunLoop().RunUntilIdle();
  }

  void CloseSpdySession(Error error, const std::string& description) {
    spdy_session_->CloseSessionOnError(error, description);
  }

  SpdyTestUtil spdy_util_;
  std::unique_ptr<SpdyProxyClientSocket> sock_;
  TestCompletionCallback read_callback_;
  TestCompletionCallback write_callback_;
  std::unique_ptr<SequencedSocketData> data_;
  BoundTestNetLog net_log_;

 private:
  std::unique_ptr<HttpNetworkSession> session_;
  scoped_refptr<IOBuffer> read_buf_;
  SpdySessionDependencies session_deps_;
  MockConnect connect_data_;
  base::WeakPtr<SpdySession> spdy_session_;
  BufferedSpdyFramer framer_;

  std::string user_agent_;
  GURL url_;
  HostPortPair proxy_host_port_;
  HostPortPair endpoint_host_port_pair_;
  ProxyServer proxy_;
  SpdySessionKey endpoint_spdy_session_key_;

  DISALLOW_COPY_AND_ASSIGN(SpdyProxyClientSocketTest);
};

INSTANTIATE_TEST_CASE_P(ProtoPlusDepend,
                        SpdyProxyClientSocketTest,
                        testing::Values(kTestCaseSPDY31,
                                        kTestCaseHTTP2NoPriorityDependencies,
                                        kTestCaseHTTP2PriorityDependencies));

SpdyProxyClientSocketTest::SpdyProxyClientSocketTest()
    : spdy_util_(GetProtocol(), GetDependenciesFromPriority()),
      read_buf_(NULL),
      session_deps_(GetProtocol()),
      connect_data_(SYNCHRONOUS, OK),
      framer_(spdy_util_.spdy_version()),
      user_agent_(kUserAgent),
      url_(kRequestUrl),
      proxy_host_port_(kProxyHost, kProxyPort),
      endpoint_host_port_pair_(kOriginHost, kOriginPort),
      proxy_(ProxyServer::SCHEME_HTTPS, proxy_host_port_),
      endpoint_spdy_session_key_(endpoint_host_port_pair_,
                                 proxy_,
                                 PRIVACY_MODE_DISABLED) {
  session_deps_.net_log = net_log_.bound().net_log();
  session_deps_.enable_priority_dependencies = GetDependenciesFromPriority();
}

SpdyProxyClientSocketTest::~SpdyProxyClientSocketTest() {
  EXPECT_TRUE(data_->AllWriteDataConsumed());
  EXPECT_TRUE(data_->AllReadDataConsumed());
}

void SpdyProxyClientSocketTest::TearDown() {
  if (session_.get() != NULL)
    session_->spdy_session_pool()->CloseAllSessions();

  // Empty the current queue.
  base::RunLoop().RunUntilIdle();
  PlatformTest::TearDown();
}

NextProto SpdyProxyClientSocketTest::GetProtocol() const {
  return GetParam() == kTestCaseSPDY31 ? kProtoSPDY31 : kProtoHTTP2;
}

bool SpdyProxyClientSocketTest::GetDependenciesFromPriority() const {
  return GetParam() == kTestCaseHTTP2PriorityDependencies;
}

void SpdyProxyClientSocketTest::Initialize(MockRead* reads,
                                           size_t reads_count,
                                           MockWrite* writes,
                                           size_t writes_count) {
  data_.reset(
      new SequencedSocketData(reads, reads_count, writes, writes_count));
  data_->set_connect_data(connect_data_);

  session_deps_.socket_factory->AddSocketDataProvider(data_.get());
  session_deps_.host_resolver->set_synchronous_mode(true);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  // Creates the SPDY session and stream.
  spdy_session_ = CreateInsecureSpdySession(
      session_.get(), endpoint_spdy_session_key_, BoundNetLog());
  base::WeakPtr<SpdyStream> spdy_stream(
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, spdy_session_, url_, LOWEST,
          net_log_.bound()));
  ASSERT_TRUE(spdy_stream.get() != NULL);

  // Create the SpdyProxyClientSocket.
  sock_.reset(new SpdyProxyClientSocket(
      spdy_stream, user_agent_, endpoint_host_port_pair_, proxy_host_port_,
      net_log_.bound(),
      new HttpAuthController(
          HttpAuth::AUTH_PROXY, GURL("https://" + proxy_host_port_.ToString()),
          session_->http_auth_cache(), session_->http_auth_handler_factory())));
}

scoped_refptr<IOBufferWithSize> SpdyProxyClientSocketTest::CreateBuffer(
    const char* data, int size) {
  scoped_refptr<IOBufferWithSize> buf(new IOBufferWithSize(size));
  memcpy(buf->data(), data, size);
  return buf;
}

void SpdyProxyClientSocketTest::AssertConnectSucceeds() {
  ASSERT_EQ(ERR_IO_PENDING, sock_->Connect(read_callback_.callback()));
  ASSERT_EQ(OK, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::AssertConnectFails(int result) {
  ASSERT_EQ(ERR_IO_PENDING, sock_->Connect(read_callback_.callback()));
  ASSERT_EQ(result, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::AssertConnectionEstablished() {
  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(200, response->headers->response_code());
}

void SpdyProxyClientSocketTest::AssertSyncReadEquals(const char* data,
                                                     int len) {
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  ASSERT_EQ(len, sock_->Read(buf.get(), len, CompletionCallback()));
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
  ASSERT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketTest::AssertAsyncReadEquals(const char* data,
                                                      int len) {
  // Issue the read, which will be completed asynchronously
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(buf.get(), len, read_callback_.callback()));
  EXPECT_TRUE(sock_->IsConnected());

  ResumeAndRun();

  EXPECT_EQ(len, read_callback_.WaitForResult());
  EXPECT_TRUE(sock_->IsConnected());
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
}

void SpdyProxyClientSocketTest::AssertReadStarts(const char* data, int len) {
  // Issue the read, which will be completed asynchronously.
  read_buf_ = new IOBuffer(len);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf_.get(), len, read_callback_.callback()));
  EXPECT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketTest::AssertReadReturns(const char* data, int len) {
  EXPECT_TRUE(sock_->IsConnected());

  // Now the read will return
  EXPECT_EQ(len, read_callback_.WaitForResult());
  ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
}

void SpdyProxyClientSocketTest::AssertAsyncWriteSucceeds(const char* data,
                                                              int len) {
  AssertWriteReturns(data, len, ERR_IO_PENDING);
  AssertWriteLength(len);
}

void SpdyProxyClientSocketTest::AssertWriteReturns(const char* data,
                                                   int len,
                                                   int rv) {
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(data, len));
  EXPECT_EQ(rv,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback()));
}

void SpdyProxyClientSocketTest::AssertWriteLength(int len) {
  EXPECT_EQ(len, write_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::PopulateConnectRequestIR(
    SpdyHeaderBlock* block) {
  spdy_util_.MaybeAddVersionHeader(block);
  (*block)[spdy_util_.GetMethodKey()] = "CONNECT";
  if (spdy_util_.spdy_version() == HTTP2) {
    (*block)[spdy_util_.GetHostKey()] = kOriginHostPort;
  } else {
    (*block)[spdy_util_.GetHostKey()] = kOriginHost;
    (*block)[spdy_util_.GetPathKey()] = kOriginHostPort;
  }
  (*block)["user-agent"] = kUserAgent;
}

void SpdyProxyClientSocketTest::PopulateConnectReplyIR(SpdyHeaderBlock* block,
                                                       const char* status) {
  (*block)[spdy_util_.GetStatusKey()] = status;
  spdy_util_.MaybeAddVersionHeader(block);
}

// Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
SpdySerializedFrame* SpdyProxyClientSocketTest::ConstructConnectRequestFrame() {
  SpdyHeaderBlock block;
  PopulateConnectRequestIR(&block);
  return spdy_util_.ConstructSpdySyn(kStreamId, std::move(block), LOWEST,
                                     false);
}

// Constructs a SPDY SYN_STREAM frame for a CONNECT request which includes
// Proxy-Authorization headers.
SpdySerializedFrame*
SpdyProxyClientSocketTest::ConstructConnectAuthRequestFrame() {
  SpdyHeaderBlock block;
  PopulateConnectRequestIR(&block);
  block["proxy-authorization"] = "Basic Zm9vOmJhcg==";
  return spdy_util_.ConstructSpdySyn(kStreamId, std::move(block), LOWEST,
                                     false);
}

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY CONNECT.
SpdySerializedFrame* SpdyProxyClientSocketTest::ConstructConnectReplyFrame() {
  SpdyHeaderBlock block;
  PopulateConnectReplyIR(&block, "200");
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY CONNECT,
// including Proxy-Authenticate headers.
SpdySerializedFrame*
SpdyProxyClientSocketTest::ConstructConnectAuthReplyFrame() {
  SpdyHeaderBlock block;
  PopulateConnectReplyIR(&block, "407");
  block["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

// Constructs a SPDY SYN_REPLY frame with an HTTP 302 redirect.
SpdySerializedFrame*
SpdyProxyClientSocketTest::ConstructConnectRedirectReplyFrame() {
  SpdyHeaderBlock block;
  PopulateConnectReplyIR(&block, "302");
  block["location"] = kRedirectUrl;
  block["set-cookie"] = "foo=bar";
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

// Constructs a SPDY SYN_REPLY frame with an HTTP 500 error.
SpdySerializedFrame*
SpdyProxyClientSocketTest::ConstructConnectErrorReplyFrame() {
  SpdyHeaderBlock block;
  PopulateConnectReplyIR(&block, "500");
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

SpdySerializedFrame* SpdyProxyClientSocketTest::ConstructBodyFrame(
    const char* data,
    int length) {
  return framer_.CreateDataFrame(kStreamId, data, length, DATA_FLAG_NONE);
}

// ----------- Connect

TEST_P(SpdyProxyClientSocketTest, ConnectSendsCorrectRequest) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_P(SpdyProxyClientSocketTest, ConnectWithAuthRequested) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectAuthReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(407, response->headers->response_code());
}

TEST_P(SpdyProxyClientSocketTest, ConnectWithAuthCredentials) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectAuthRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));
  AddAuthToCache();

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_P(SpdyProxyClientSocketTest, ConnectRedirects) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(
      ConstructConnectRedirectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);

  const HttpResponseHeaders* headers = response->headers.get();
  ASSERT_EQ(302, headers->response_code());
  ASSERT_FALSE(headers->HasHeader("set-cookie"));
  ASSERT_TRUE(headers->HasHeaderValue("content-length", "0"));

  std::string location;
  ASSERT_TRUE(headers->IsRedirect(&location));
  ASSERT_EQ(location, kRedirectUrl);

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

TEST_P(SpdyProxyClientSocketTest, ConnectFails) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    MockRead(ASYNC, 0, 1),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_CONNECTION_CLOSED);

  ASSERT_FALSE(sock_->IsConnected());
}

// ----------- WasEverUsed

TEST_P(SpdyProxyClientSocketTest, WasEverUsedReturnsCorrectValues) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_FALSE(sock_->WasEverUsed());
  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->WasEverUsed());
  sock_->Disconnect();
  EXPECT_TRUE(sock_->WasEverUsed());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// ----------- GetPeerAddress

TEST_P(SpdyProxyClientSocketTest, GetPeerAddressReturnsCorrectValues) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  IPEndPoint addr;
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));

  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(OK, sock_->GetPeerAddress(&addr));

  ResumeAndRun();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));

  sock_->Disconnect();

  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));
}

// ----------- Write

TEST_P(SpdyProxyClientSocketTest, WriteSendsDataInDataFrame) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS),
      CreateMockWrite(*msg1, 3, SYNCHRONOUS),
      CreateMockWrite(*msg2, 4, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  AssertAsyncWriteSucceeds(kMsg1, kLen1);
  AssertAsyncWriteSucceeds(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, WriteSplitsLargeDataIntoMultipleFrames) {
  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> chunk(
      ConstructBodyFrame(chunk_data.data(), chunk_data.length()));
  MockWrite writes[] = {CreateMockWrite(*conn, 0, SYNCHRONOUS),
                        CreateMockWrite(*chunk, 3, SYNCHRONOUS),
                        CreateMockWrite(*chunk, 4, SYNCHRONOUS),
                        CreateMockWrite(*chunk, 5, SYNCHRONOUS)};

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  std::string big_data(kMaxSpdyFrameChunkSize * 3, 'x');
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(big_data.data(),
                                                   big_data.length()));

  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback()));
  EXPECT_EQ(buf->size(), write_callback_.WaitForResult());
}

// ----------- Read

TEST_P(SpdyProxyClientSocketTest, ReadReadsDataInDataFrame) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_P(SpdyProxyClientSocketTest, ReadDataFromBufferedFrames) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 4),
      CreateMockRead(*msg2, 5, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, ReadDataMultipleBufferedFrames) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC),
      CreateMockRead(*msg2, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest,
       LargeReadWillMergeDataFromDifferentFrames) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg3, 3, ASYNC),
      CreateMockRead(*msg3, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
}

TEST_P(SpdyProxyClientSocketTest, MultipleShortReadsThenMoreRead) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC),
      CreateMockRead(*msg3, 4, ASYNC),
      CreateMockRead(*msg3, 5, ASYNC),
      CreateMockRead(*msg2, 6, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 7),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next four reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, ReadWillSplitDataFromLargeFrame) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg33(
      ConstructBodyFrame(kMsg33, kLen33));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC),
      CreateMockRead(*msg33, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
}

TEST_P(SpdyProxyClientSocketTest, MultipleReadsFromSameLargeFrame) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg333(
      ConstructBodyFrame(kMsg333, kLen333));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg333, 3, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33, kLen33);

  // Now attempt to do a read of more data than remains buffered
  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen33));
  ASSERT_EQ(kLen3, sock_->Read(buf.get(), kLen33, read_callback_.callback()));
  ASSERT_EQ(std::string(kMsg3, kLen3), std::string(buf->data(), kLen3));
  ASSERT_TRUE(sock_->IsConnected());
}

TEST_P(SpdyProxyClientSocketTest, ReadAuthResponseBody) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectAuthReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC),
      CreateMockRead(*msg2, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, ReadErrorResponseBody) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectErrorReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    CreateMockRead(*msg1, 2, ASYNC),
    CreateMockRead(*msg2, 3, ASYNC),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
}

// ----------- Reads and Writes

TEST_P(SpdyProxyClientSocketTest, AsyncReadAroundWrite) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS),
      CreateMockWrite(*msg2, 4, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC),  // sync read
      MockRead(ASYNC, ERR_IO_PENDING, 5),
      CreateMockRead(*msg3, 6, ASYNC),  // async read
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 7),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  AssertReadStarts(kMsg3, kLen3);
  // Read should block until after the write succeeds.

  AssertAsyncWriteSucceeds(kMsg2, kLen2);  // Advances past paused read.

  ASSERT_FALSE(read_callback_.have_result());
  ResumeAndRun();
  // Now the read will return.
  AssertReadReturns(kMsg3, kLen3);
}

TEST_P(SpdyProxyClientSocketTest, AsyncWriteAroundReads) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS),
      MockWrite(ASYNC, ERR_IO_PENDING, 7), CreateMockWrite(*msg2, 8, ASYNC),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  std::unique_ptr<SpdySerializedFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 4),
      CreateMockRead(*msg3, 5, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // Write should block until the read completes
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  AssertAsyncReadEquals(kMsg3, kLen3);

  ASSERT_FALSE(write_callback_.have_result());

  // Now the write will complete
  ResumeAndRun();
  AssertWriteLength(kLen2);
}

// ----------- Reading/Writing on Closed socket

// Reading from an already closed socket should return 0
TEST_P(SpdyProxyClientSocketTest, ReadOnClosedSocketReturnsZero) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  ResumeAndRun();

  ASSERT_FALSE(sock_->IsConnected());
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

// Read pending when socket is closed should return 0
TEST_P(SpdyProxyClientSocketTest, PendingReadOnCloseReturnsZero) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1, kLen1);

  ResumeAndRun();

  ASSERT_EQ(0, read_callback_.WaitForResult());
}

// Reading from a disconnected socket is an error
TEST_P(SpdyProxyClientSocketTest,
       ReadOnDisconnectSocketReturnsNotConnected) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  sock_->Disconnect();

  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(NULL, 1, CompletionCallback()));

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// Reading buffered data from an already closed socket should return
// buffered data, then 0.
TEST_P(SpdyProxyClientSocketTest, ReadOnClosedSocketReturnsBufferedData) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC), MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  ResumeAndRun();

  ASSERT_FALSE(sock_->IsConnected());
  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen1));
  ASSERT_EQ(kLen1, sock_->Read(buf.get(), kLen1, CompletionCallback()));
  ASSERT_EQ(std::string(kMsg1, kLen1), std::string(buf->data(), kLen1));

  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  ASSERT_EQ(0, sock_->Read(NULL, 1, CompletionCallback()));
  sock_->Disconnect();
  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(NULL, 1, CompletionCallback()));
}

// Calling Write() on a closed socket is an error
TEST_P(SpdyProxyClientSocketTest, WriteOnClosedStream) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // Read EOF which will close the stream.
  ResumeAndRun();
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Write(buf.get(), buf->size(), CompletionCallback()));
}

// Calling Write() on a disconnected socket is an error.
TEST_P(SpdyProxyClientSocketTest, WriteOnDisconnectedSocket) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  sock_->Disconnect();

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Write(buf.get(), buf->size(), CompletionCallback()));

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// If the socket is closed with a pending Write(), the callback
// should be called with ERR_CONNECTION_CLOSED.
TEST_P(SpdyProxyClientSocketTest, WritePendingOnClose) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS),
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback()));
  // Make sure the write actually starts.
  base::RunLoop().RunUntilIdle();

  CloseSpdySession(ERR_ABORTED, std::string());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, write_callback_.WaitForResult());
}

// If the socket is Disconnected with a pending Write(), the callback
// should not be called.
TEST_P(SpdyProxyClientSocketTest, DisconnectWithWritePending) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback()));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(write_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// If the socket is Disconnected with a pending Read(), the callback
// should not be called.
TEST_P(SpdyProxyClientSocketTest, DisconnectWithReadPending) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 3),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(buf.get(), kLen1, read_callback_.callback()));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(read_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// If the socket is Reset when both a read and write are pending,
// both should be called back.
TEST_P(SpdyProxyClientSocketTest, RstWithReadAndWritePending) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*rst, 3, ASYNC), MockRead(ASYNC, 0, 4)  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBuffer> read_buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), kLen1, read_callback_.callback()));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(
      ERR_IO_PENDING,
      sock_->Write(
          write_buf.get(), write_buf->size(), write_callback_.callback()));

  ResumeAndRun();

  EXPECT_TRUE(sock_.get());
  EXPECT_TRUE(read_callback_.have_result());
  EXPECT_TRUE(write_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// Makes sure the proxy client socket's source gets the expected NetLog events
// and only the expected NetLog events (No SpdySession events).
TEST_P(SpdyProxyClientSocketTest, NetLog) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*conn, 0, SYNCHRONOUS), CreateMockWrite(*rst, 5),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*msg1, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  NetLog::Source sock_source = sock_->NetLog().source();
  sock_.reset();

  TestNetLogEntry::List entry_list;
  net_log_.GetEntriesForSource(sock_source, &entry_list);

  ASSERT_EQ(entry_list.size(), 10u);
  EXPECT_TRUE(LogContainsBeginEvent(entry_list, 0, NetLog::TYPE_SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(entry_list, 1,
                               NetLog::TYPE_HTTP2_PROXY_CLIENT_SESSION,
                               NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsBeginEvent(entry_list, 2,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsEvent(entry_list, 3,
                  NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(entry_list, 4,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsBeginEvent(entry_list, 5,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 6,
                  NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(entry_list, 7,
                  NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 8,
                  NetLog::TYPE_SOCKET_BYTES_RECEIVED,
                  NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(entry_list, 9, NetLog::TYPE_SOCKET_ALIVE));

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// CompletionCallback that causes the SpdyProxyClientSocket to be
// deleted when Run is invoked.
class DeleteSockCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSockCallback(std::unique_ptr<SpdyProxyClientSocket>* sock)
      : sock_(sock),
        callback_(base::Bind(&DeleteSockCallback::OnComplete,
                             base::Unretained(this))) {}

  ~DeleteSockCallback() override {}

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    sock_->reset(NULL);
    SetResult(result);
  }

  std::unique_ptr<SpdyProxyClientSocket>* sock_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSockCallback);
};

// If the socket is Reset when both a read and write are pending, and the
// read callback causes the socket to be deleted, the write callback should
// not be called.
TEST_P(SpdyProxyClientSocketTest, RstWithReadAndWritePendingDelete) {
  std::unique_ptr<SpdySerializedFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, SYNCHRONOUS),
  };

  std::unique_ptr<SpdySerializedFrame> resp(ConstructConnectReplyFrame());
  std::unique_ptr<SpdySerializedFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockRead reads[] = {
      CreateMockRead(*resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(*rst, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  DeleteSockCallback read_callback(&sock_);

  scoped_refptr<IOBuffer> read_buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), kLen1, read_callback.callback()));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(
      ERR_IO_PENDING,
      sock_->Write(
          write_buf.get(), write_buf->size(), write_callback_.callback()));

  ResumeAndRun();

  EXPECT_FALSE(sock_.get());
  EXPECT_TRUE(read_callback.have_result());
  EXPECT_FALSE(write_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

}  // namespace net
