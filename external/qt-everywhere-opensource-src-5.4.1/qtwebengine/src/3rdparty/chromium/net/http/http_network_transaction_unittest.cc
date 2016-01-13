// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include <math.h>  // ceil
#include <stdarg.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "net/base/auth.h"
#include "net/base/capturing_net_log.h"
#include "net/base/completion_callback.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/host_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_digest.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

//-----------------------------------------------------------------------------

namespace {

const base::string16 kBar(ASCIIToUTF16("bar"));
const base::string16 kBar2(ASCIIToUTF16("bar2"));
const base::string16 kBar3(ASCIIToUTF16("bar3"));
const base::string16 kBaz(ASCIIToUTF16("baz"));
const base::string16 kFirst(ASCIIToUTF16("first"));
const base::string16 kFoo(ASCIIToUTF16("foo"));
const base::string16 kFoo2(ASCIIToUTF16("foo2"));
const base::string16 kFoo3(ASCIIToUTF16("foo3"));
const base::string16 kFou(ASCIIToUTF16("fou"));
const base::string16 kSecond(ASCIIToUTF16("second"));
const base::string16 kTestingNTLM(ASCIIToUTF16("testing-ntlm"));
const base::string16 kWrongPassword(ASCIIToUTF16("wrongpassword"));

int GetIdleSocketCountInTransportSocketPool(net::HttpNetworkSession* session) {
  return session->GetTransportSocketPool(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL)->IdleSocketCount();
}

int GetIdleSocketCountInSSLSocketPool(net::HttpNetworkSession* session) {
  return session->GetSSLSocketPool(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL)->IdleSocketCount();
}

bool IsTransportSocketPoolStalled(net::HttpNetworkSession* session) {
  return session->GetTransportSocketPool(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL)->IsStalled();
}

// Takes in a Value created from a NetLogHttpResponseParameter, and returns
// a JSONified list of headers as a single string.  Uses single quotes instead
// of double quotes for easier comparison.  Returns false on failure.
bool GetHeaders(base::DictionaryValue* params, std::string* headers) {
  if (!params)
    return false;
  base::ListValue* header_list;
  if (!params->GetList("headers", &header_list))
    return false;
  std::string double_quote_headers;
  base::JSONWriter::Write(header_list, &double_quote_headers);
  base::ReplaceChars(double_quote_headers, "\"", "'", headers);
  return true;
}

// Tests LoadTimingInfo in the case a socket is reused and no PAC script is
// used.
void TestLoadTimingReused(const net::LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());

  net::ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  EXPECT_FALSE(load_timing_info.send_start.is_null());

  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);

  // Set at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Tests LoadTimingInfo in the case a new socket is used and no PAC script is
// used.
void TestLoadTimingNotReused(const net::LoadTimingInfo& load_timing_info,
                             int connect_timing_flags) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());

  net::ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                                   connect_timing_flags);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);

  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);

  // Set at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Tests LoadTimingInfo in the case a socket is reused and a PAC script is
// used.
void TestLoadTimingReusedWithPac(const net::LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  net::ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  EXPECT_FALSE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);

  // Set at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Tests LoadTimingInfo in the case a new socket is used and a PAC script is
// used.
void TestLoadTimingNotReusedWithPac(const net::LoadTimingInfo& load_timing_info,
                                    int connect_timing_flags) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.connect_timing.connect_start);
  net::ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                                   connect_timing_flags);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);

  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);

  // Set at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

}  // namespace

namespace net {

namespace {

HttpNetworkSession* CreateSession(SpdySessionDependencies* session_deps) {
  return SpdySessionDependencies::SpdyCreateSession(session_deps);
}

}  // namespace

class HttpNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<NextProto> {
 public:
  virtual ~HttpNetworkTransactionTest() {
    // Important to restore the per-pool limit first, since the pool limit must
    // always be greater than group limit, and the tests reduce both limits.
    ClientSocketPoolManager::set_max_sockets_per_pool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_pool_sockets_);
    ClientSocketPoolManager::set_max_sockets_per_group(
        HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_group_sockets_);
  }

 protected:
  HttpNetworkTransactionTest()
      : spdy_util_(GetParam()),
        session_deps_(GetParam()),
        old_max_group_sockets_(ClientSocketPoolManager::max_sockets_per_group(
            HttpNetworkSession::NORMAL_SOCKET_POOL)),
        old_max_pool_sockets_(ClientSocketPoolManager::max_sockets_per_pool(
            HttpNetworkSession::NORMAL_SOCKET_POOL)) {
  }

  struct SimpleGetHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
    int64 totalReceivedBytes;
    LoadTimingInfo load_timing_info;
  };

  virtual void SetUp() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  virtual void TearDown() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
    // Empty the current queue.
    base::MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::MessageLoop::current()->RunUntilIdle();
  }

  // This is the expected return from a current server advertising SPDY.
  std::string GetAlternateProtocolHttpHeader() {
    return
        std::string("Alternate-Protocol: 443:") +
        AlternateProtocolToString(AlternateProtocolFromNextProto(GetParam())) +
        "\r\n\r\n";
  }

  // Either |write_failure| specifies a write failure or |read_failure|
  // specifies a read failure when using a reused socket.  In either case, the
  // failure should cause the network transaction to resend the request, and the
  // other argument should be NULL.
  void KeepAliveConnectionResendRequestTest(const MockWrite* write_failure,
                                            const MockRead* read_failure);

  // Either |write_failure| specifies a write failure or |read_failure|
  // specifies a read failure when using a reused socket.  In either case, the
  // failure should cause the network transaction to resend the request, and the
  // other argument should be NULL.
  void PreconnectErrorResendRequestTest(const MockWrite* write_failure,
                                        const MockRead* read_failure,
                                        bool use_spdy);

  SimpleGetHelperResult SimpleGetHelperForData(StaticSocketDataProvider* data[],
                                               size_t data_count) {
    SimpleGetHelperResult out;

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    CapturingBoundNetLog log;
    session_deps_.net_log = log.bound().net_log();
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

    for (size_t i = 0; i < data_count; ++i) {
      session_deps_.socket_factory->AddSocketDataProvider(data[i]);
    }

    TestCompletionCallback callback;

    EXPECT_TRUE(log.bound().IsLogging());
    int rv = trans->Start(&request, callback.callback(), log.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    out.rv = callback.WaitForResult();

    // Even in the failure cases that use this function, connections are always
    // successfully established before the error.
    EXPECT_TRUE(trans->GetLoadTimingInfo(&out.load_timing_info));
    TestLoadTimingNotReused(out.load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);

    if (out.rv != OK)
      return out;

    const HttpResponseInfo* response = trans->GetResponseInfo();
    // Can't use ASSERT_* inside helper functions like this, so
    // return an error.
    if (response == NULL || response->headers.get() == NULL) {
      out.rv = ERR_UNEXPECTED;
      return out;
    }
    out.status_line = response->headers->GetStatusLine();

    EXPECT_EQ("127.0.0.1", response->socket_address.host());
    EXPECT_EQ(80, response->socket_address.port());

    rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, rv);

    net::CapturingNetLog::CapturedEntryList entries;
    log.GetEntries(&entries);
    size_t pos = ExpectLogContainsSomewhere(
        entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
        NetLog::PHASE_NONE);
    ExpectLogContainsSomewhere(
        entries, pos,
        NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
        NetLog::PHASE_NONE);

    std::string line;
    EXPECT_TRUE(entries[pos].GetStringValue("line", &line));
    EXPECT_EQ("GET / HTTP/1.1\r\n", line);

    HttpRequestHeaders request_headers;
    EXPECT_TRUE(trans->GetFullRequestHeaders(&request_headers));
    std::string value;
    EXPECT_TRUE(request_headers.GetHeader("Host", &value));
    EXPECT_EQ("www.google.com", value);
    EXPECT_TRUE(request_headers.GetHeader("Connection", &value));
    EXPECT_EQ("keep-alive", value);

    std::string response_headers;
    EXPECT_TRUE(GetHeaders(entries[pos].params.get(), &response_headers));
    EXPECT_EQ("['Host: www.google.com','Connection: keep-alive']",
              response_headers);

    out.totalReceivedBytes = trans->GetTotalReceivedBytes();
    return out;
  }

  SimpleGetHelperResult SimpleGetHelper(MockRead data_reads[],
                                        size_t reads_count) {
    StaticSocketDataProvider reads(data_reads, reads_count, NULL, 0);
    StaticSocketDataProvider* data[] = { &reads };
    return SimpleGetHelperForData(data, 1);
  }

  int64 ReadsSize(MockRead data_reads[], size_t reads_count) {
    int64 size = 0;
    for (size_t i = 0; i < reads_count; ++i)
      size += data_reads[i].data_len;
    return size;
  }

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);

  void BypassHostCacheOnRefreshHelper(int load_flags);

  void CheckErrorIsPassedBack(int error, IoMode mode);

  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;

  // Original socket limits.  Some tests set these.  Safest to always restore
  // them once each test has been run.
  int old_max_group_sockets_;
  int old_max_pool_sockets_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    HttpNetworkTransactionTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

namespace {

class BeforeNetworkStartHandler {
 public:
  explicit BeforeNetworkStartHandler(bool defer)
      : defer_on_before_network_start_(defer),
        observed_before_network_start_(false) {}

  void OnBeforeNetworkStart(bool* defer) {
    *defer = defer_on_before_network_start_;
    observed_before_network_start_ = true;
  }

  bool observed_before_network_start() const {
    return observed_before_network_start_;
  }

 private:
  const bool defer_on_before_network_start_;
  bool observed_before_network_start_;

  DISALLOW_COPY_AND_ASSIGN(BeforeNetworkStartHandler);
};

// Fill |str| with a long header list that consumes >= |size| bytes.
void FillLargeHeadersString(std::string* str, int size) {
  const char* row =
      "SomeHeaderName: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n";
  const int sizeof_row = strlen(row);
  const int num_rows = static_cast<int>(
      ceil(static_cast<float>(size) / sizeof_row));
  const int sizeof_data = num_rows * sizeof_row;
  DCHECK(sizeof_data >= size);
  str->reserve(sizeof_data);

  for (int i = 0; i < num_rows; ++i)
    str->append(row, sizeof_row);
}

// Alternative functions that eliminate randomness and dependency on the local
// host name so that the generated NTLM messages are reproducible.
void MockGenerateRandom1(uint8* output, size_t n) {
  static const uint8 bytes[] = {
    0x55, 0x29, 0x66, 0x26, 0x6b, 0x9c, 0x73, 0x54
  };
  static size_t current_byte = 0;
  for (size_t i = 0; i < n; ++i) {
    output[i] = bytes[current_byte++];
    current_byte %= arraysize(bytes);
  }
}

void MockGenerateRandom2(uint8* output, size_t n) {
  static const uint8 bytes[] = {
    0x96, 0x79, 0x85, 0xe7, 0x49, 0x93, 0x70, 0xa1,
    0x4e, 0xe7, 0x87, 0x45, 0x31, 0x5b, 0xd3, 0x1f
  };
  static size_t current_byte = 0;
  for (size_t i = 0; i < n; ++i) {
    output[i] = bytes[current_byte++];
    current_byte %= arraysize(bytes);
  }
}

std::string MockGetHostName() {
  return "WTC-WIN7";
}

template<typename ParentPool>
class CaptureGroupNameSocketPool : public ParentPool {
 public:
  CaptureGroupNameSocketPool(HostResolver* host_resolver,
                             CertVerifier* cert_verifier);

  const std::string last_group_name_received() const {
    return last_group_name_;
  }

  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            const CompletionCallback& callback,
                            const BoundNetLog& net_log) {
    last_group_name_ = group_name;
    return ERR_IO_PENDING;
  }
  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) {}
  virtual void ReleaseSocket(const std::string& group_name,
                             scoped_ptr<StreamSocket> socket,
                             int id) {}
  virtual void CloseIdleSockets() {}
  virtual int IdleSocketCount() const {
    return 0;
  }
  virtual int IdleSocketCountInGroup(const std::string& group_name) const {
    return 0;
  }
  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const {
    return LOAD_STATE_IDLE;
  }
  virtual base::TimeDelta ConnectionTimeout() const {
    return base::TimeDelta();
  }

 private:
  std::string last_group_name_;
};

typedef CaptureGroupNameSocketPool<TransportClientSocketPool>
CaptureGroupNameTransportSocketPool;
typedef CaptureGroupNameSocketPool<HttpProxyClientSocketPool>
CaptureGroupNameHttpProxySocketPool;
typedef CaptureGroupNameSocketPool<SOCKSClientSocketPool>
CaptureGroupNameSOCKSSocketPool;
typedef CaptureGroupNameSocketPool<SSLClientSocketPool>
CaptureGroupNameSSLSocketPool;

template<typename ParentPool>
CaptureGroupNameSocketPool<ParentPool>::CaptureGroupNameSocketPool(
    HostResolver* host_resolver,
    CertVerifier* /* cert_verifier */)
    : ParentPool(0, 0, NULL, host_resolver, NULL, NULL) {}

template<>
CaptureGroupNameHttpProxySocketPool::CaptureGroupNameSocketPool(
    HostResolver* host_resolver,
    CertVerifier* /* cert_verifier */)
    : HttpProxyClientSocketPool(0, 0, NULL, host_resolver, NULL, NULL, NULL) {}

template <>
CaptureGroupNameSSLSocketPool::CaptureGroupNameSocketPool(
    HostResolver* host_resolver,
    CertVerifier* cert_verifier)
    : SSLClientSocketPool(0,
                          0,
                          NULL,
                          host_resolver,
                          cert_verifier,
                          NULL,
                          NULL,
                          NULL,
                          std::string(),
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL) {}

//-----------------------------------------------------------------------------

// Helper functions for validating that AuthChallengeInfo's are correctly
// configured for common cases.
bool CheckBasicServerAuth(const AuthChallengeInfo* auth_challenge) {
  if (!auth_challenge)
    return false;
  EXPECT_FALSE(auth_challenge->is_proxy);
  EXPECT_EQ("www.google.com:80", auth_challenge->challenger.ToString());
  EXPECT_EQ("MyRealm1", auth_challenge->realm);
  EXPECT_EQ("basic", auth_challenge->scheme);
  return true;
}

bool CheckBasicProxyAuth(const AuthChallengeInfo* auth_challenge) {
  if (!auth_challenge)
    return false;
  EXPECT_TRUE(auth_challenge->is_proxy);
  EXPECT_EQ("myproxy:70", auth_challenge->challenger.ToString());
  EXPECT_EQ("MyRealm1", auth_challenge->realm);
  EXPECT_EQ("basic", auth_challenge->scheme);
  return true;
}

bool CheckDigestServerAuth(const AuthChallengeInfo* auth_challenge) {
  if (!auth_challenge)
    return false;
  EXPECT_FALSE(auth_challenge->is_proxy);
  EXPECT_EQ("www.google.com:80", auth_challenge->challenger.ToString());
  EXPECT_EQ("digestive", auth_challenge->realm);
  EXPECT_EQ("digest", auth_challenge->scheme);
  return true;
}

bool CheckNTLMServerAuth(const AuthChallengeInfo* auth_challenge) {
  if (!auth_challenge)
    return false;
  EXPECT_FALSE(auth_challenge->is_proxy);
  EXPECT_EQ("172.22.68.17:80", auth_challenge->challenger.ToString());
  EXPECT_EQ(std::string(), auth_challenge->realm);
  EXPECT_EQ("ntlm", auth_challenge->scheme);
  return true;
}

}  // namespace

TEST_P(HttpNetworkTransactionTest, Basic) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
}

TEST_P(HttpNetworkTransactionTest, SimpleGET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 200 OK", out.status_line);
  EXPECT_EQ("hello world", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Response with no status line.
TEST_P(HttpNetworkTransactionTest, SimpleGETNoHeaders) {
  MockRead data_reads[] = {
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("hello world", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Allow up to 4 bytes of junk to precede status line.
TEST_P(HttpNetworkTransactionTest, StatusLineJunk3Bytes) {
  MockRead data_reads[] = {
    MockRead("xxxHTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Allow up to 4 bytes of junk to precede status line.
TEST_P(HttpNetworkTransactionTest, StatusLineJunk4Bytes) {
  MockRead data_reads[] = {
    MockRead("\n\nQJHTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Beyond 4 bytes of slop and it should fail to find a status line.
TEST_P(HttpNetworkTransactionTest, StatusLineJunk5Bytes) {
  MockRead data_reads[] = {
    MockRead("xxxxxHTTP/1.1 404 Not Found\nServer: blah"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("xxxxxHTTP/1.1 404 Not Found\nServer: blah", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Same as StatusLineJunk4Bytes, except the read chunks are smaller.
TEST_P(HttpNetworkTransactionTest, StatusLineJunk4Bytes_Slow) {
  MockRead data_reads[] = {
    MockRead("\n"),
    MockRead("\n"),
    MockRead("Q"),
    MockRead("J"),
    MockRead("HTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Close the connection before enough bytes to have a status line.
TEST_P(HttpNetworkTransactionTest, StatusLinePartial) {
  MockRead data_reads[] = {
    MockRead("HTT"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("HTT", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, out.totalReceivedBytes);
}

// Simulate a 204 response, lacking a Content-Length header, sent over a
// persistent connection.  The response should still terminate since a 204
// cannot have a response body.
TEST_P(HttpNetworkTransactionTest, StopsReading204) {
  char junk[] = "junk";
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 204 No Content\r\n\r\n"),
    MockRead(junk),  // Should not be read!!
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 204 No Content", out.status_line);
  EXPECT_EQ("", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  int64 response_size = reads_size - strlen(junk);
  EXPECT_EQ(response_size, out.totalReceivedBytes);
}

// A simple request using chunked encoding with some extra data after.
TEST_P(HttpNetworkTransactionTest, ChunkedEncoding) {
  std::string final_chunk = "0\r\n\r\n";
  std::string extra_data = "HTTP/1.1 200 OK\r\n";
  std::string last_read = final_chunk + extra_data;
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"),
    MockRead("5\r\nHello\r\n"),
    MockRead("1\r\n"),
    MockRead(" \r\n"),
    MockRead("5\r\nworld\r\n"),
    MockRead(last_read.data()),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("Hello world", out.response_data);
  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  int64 response_size = reads_size - extra_data.size();
  EXPECT_EQ(response_size, out.totalReceivedBytes);
}

// Next tests deal with http://crbug.com/56344.

TEST_P(HttpNetworkTransactionTest,
       MultipleContentLengthHeadersNoTransferEncoding) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 10\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH, out.rv);
}

TEST_P(HttpNetworkTransactionTest,
       DuplicateContentLengthHeadersNoTransferEncoding) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 5\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("Hello"),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("Hello", out.response_data);
}

TEST_P(HttpNetworkTransactionTest,
       ComplexContentLengthHeadersNoTransferEncoding) {
  // More than 2 dupes.
  {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Content-Length: 5\r\n"),
      MockRead("Content-Length: 5\r\n"),
      MockRead("Content-Length: 5\r\n\r\n"),
      MockRead("Hello"),
    };
    SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                                arraysize(data_reads));
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("Hello", out.response_data);
  }
  // HTTP/1.0
  {
    MockRead data_reads[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 5\r\n"),
      MockRead("Content-Length: 5\r\n"),
      MockRead("Content-Length: 5\r\n\r\n"),
      MockRead("Hello"),
    };
    SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                                arraysize(data_reads));
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.0 200 OK", out.status_line);
    EXPECT_EQ("Hello", out.response_data);
  }
  // 2 dupes and one mismatched.
  {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Content-Length: 10\r\n"),
      MockRead("Content-Length: 10\r\n"),
      MockRead("Content-Length: 5\r\n\r\n"),
    };
    SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                                arraysize(data_reads));
    EXPECT_EQ(ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH, out.rv);
  }
}

TEST_P(HttpNetworkTransactionTest,
       MultipleContentLengthHeadersTransferEncoding) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 666\r\n"),
    MockRead("Content-Length: 1337\r\n"),
    MockRead("Transfer-Encoding: chunked\r\n\r\n"),
    MockRead("5\r\nHello\r\n"),
    MockRead("1\r\n"),
    MockRead(" \r\n"),
    MockRead("5\r\nworld\r\n"),
    MockRead("0\r\n\r\nHTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("Hello world", out.response_data);
}

// Next tests deal with http://crbug.com/98895.

// Checks that a single Content-Disposition header results in no error.
TEST_P(HttpNetworkTransactionTest, SingleContentDispositionHeader) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Disposition: attachment;filename=\"salutations.txt\"r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("Hello"),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("Hello", out.response_data);
}

// Checks that two identical Content-Disposition headers result in no error.
TEST_P(HttpNetworkTransactionTest,
       TwoIdenticalContentDispositionHeaders) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Disposition: attachment;filename=\"greetings.txt\"r\n"),
    MockRead("Content-Disposition: attachment;filename=\"greetings.txt\"r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("Hello"),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("Hello", out.response_data);
}

// Checks that two distinct Content-Disposition headers result in an error.
TEST_P(HttpNetworkTransactionTest, TwoDistinctContentDispositionHeaders) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Disposition: attachment;filename=\"greetings.txt\"r\n"),
    MockRead("Content-Disposition: attachment;filename=\"hi.txt\"r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("Hello"),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION, out.rv);
}

// Checks that two identical Location headers result in no error.
// Also tests Location header behavior.
TEST_P(HttpNetworkTransactionTest, TwoIdenticalLocationHeaders) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 302 Redirect\r\n"),
    MockRead("Location: http://good.com/\r\n"),
    MockRead("Location: http://good.com/\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://redirect.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL && response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 302 Redirect", response->headers->GetStatusLine());
  std::string url;
  EXPECT_TRUE(response->headers->IsRedirect(&url));
  EXPECT_EQ("http://good.com/", url);
  EXPECT_TRUE(response->proxy_server.IsEmpty());
}

// Checks that two distinct Location headers result in an error.
TEST_P(HttpNetworkTransactionTest, TwoDistinctLocationHeaders) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 302 Redirect\r\n"),
    MockRead("Location: http://good.com/\r\n"),
    MockRead("Location: http://evil.com/\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION, out.rv);
}

// Do a request using the HEAD method. Verify that we don't try to read the
// message body (since HEAD has none).
TEST_P(HttpNetworkTransactionTest, Head) {
  HttpRequestInfo request;
  request.method = "HEAD";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes1[] = {
    MockWrite("HEAD / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 404 Not Found\r\n"),
    MockRead("Server: Blah\r\n"),
    MockRead("Content-Length: 1234\r\n\r\n"),

    // No response body because the test stops reading here.
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  // Check that the headers got parsed.
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ(1234, response->headers->GetContentLength());
  EXPECT_EQ("HTTP/1.1 404 Not Found", response->headers->GetStatusLine());
  EXPECT_TRUE(response->proxy_server.IsEmpty());

  std::string server_header;
  void* iter = NULL;
  bool has_server_header = response->headers->EnumerateHeader(
      &iter, "Server", &server_header);
  EXPECT_TRUE(has_server_header);
  EXPECT_EQ("Blah", server_header);

  // Reading should give EOF right away, since there is no message body
  // (despite non-zero content-length).
  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("", response_data);
}

TEST_P(HttpNetworkTransactionTest, ReuseConnection) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  const char* const kExpectedResponseData[] = {
    "hello", "world"
  };

  for (int i = 0; i < 2; ++i) {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    TestCompletionCallback callback;

    int rv = trans->Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_TRUE(response->proxy_server.IsEmpty());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

TEST_P(HttpNetworkTransactionTest, Ignores100) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 100 Continue\r\n\r\n"),
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// This test is almost the same as Ignores100 above, but the response contains
// a 102 instead of a 100. Also, instead of HTTP/1.0 the response is
// HTTP/1.1 and the two status headers are read in one read.
TEST_P(HttpNetworkTransactionTest, Ignores1xx) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 102 Unspecified status code\r\n\r\n"
             "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

TEST_P(HttpNetworkTransactionTest, Incomplete100ThenEOF) {
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockRead data_reads[] = {
    MockRead(SYNCHRONOUS, "HTTP/1.0 100 Continue\r\n"),
    MockRead(ASYNC, 0),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("", response_data);
}

TEST_P(HttpNetworkTransactionTest, EmptyResponse) {
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));


  MockRead data_reads[] = {
    MockRead(ASYNC, 0),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_EMPTY_RESPONSE, rv);
}

void HttpNetworkTransactionTest::KeepAliveConnectionResendRequestTest(
    const MockWrite* write_failure,
    const MockRead* read_failure) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Written data for successfully sending both requests.
  MockWrite data1_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  // Read results for the first request.
  MockRead data1_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
    MockRead(ASYNC, OK),
  };

  if (write_failure) {
    ASSERT_FALSE(read_failure);
    data1_writes[1] = *write_failure;
  } else {
    ASSERT_TRUE(read_failure);
    data1_reads[2] = *read_failure;
  }

  StaticSocketDataProvider data1(data1_reads, arraysize(data1_reads),
                                 data1_writes, arraysize(data1_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  MockRead data2_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider data2(data2_reads, arraysize(data2_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  const char* kExpectedResponseData[] = {
    "hello", "world"
  };

  uint32 first_socket_log_id = NetLog::Source::kInvalidId;
  for (int i = 0; i < 2; ++i) {
    TestCompletionCallback callback;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    int rv = trans->Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    LoadTimingInfo load_timing_info;
    EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
    if (i == 0) {
      first_socket_log_id = load_timing_info.socket_log_id;
    } else {
      // The second request should be using a new socket.
      EXPECT_NE(first_socket_log_id, load_timing_info.socket_log_id);
    }

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

void HttpNetworkTransactionTest::PreconnectErrorResendRequestTest(
    const MockWrite* write_failure,
    const MockRead* read_failure,
    bool use_spdy) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.foo.com/");
  request.load_flags = 0;

  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  SSLSocketDataProvider ssl1(ASYNC, OK);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  if (use_spdy) {
    ssl1.SetNextProto(GetParam());
    ssl2.SetNextProto(GetParam());
  }
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl1);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl2);

  // SPDY versions of the request and response.
  scoped_ptr<SpdyFrame> spdy_request(spdy_util_.ConstructSpdyGet(
      request.url.spec().c_str(), false, 1, DEFAULT_PRIORITY));
  scoped_ptr<SpdyFrame> spdy_response(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> spdy_data(
      spdy_util_.ConstructSpdyBodyFrame(1, "hello", 5, true));

  // HTTP/1.1 versions of the request and response.
  const char kHttpRequest[] = "GET / HTTP/1.1\r\n"
      "Host: www.foo.com\r\n"
      "Connection: keep-alive\r\n\r\n";
  const char kHttpResponse[] = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n";
  const char kHttpData[] = "hello";

  std::vector<MockRead> data1_reads;
  std::vector<MockWrite> data1_writes;
  if (write_failure) {
    ASSERT_FALSE(read_failure);
    data1_writes.push_back(*write_failure);
    data1_reads.push_back(MockRead(ASYNC, OK));
  } else {
    ASSERT_TRUE(read_failure);
    if (use_spdy) {
      data1_writes.push_back(CreateMockWrite(*spdy_request));
    } else {
      data1_writes.push_back(MockWrite(kHttpRequest));
    }
    data1_reads.push_back(*read_failure);
  }

  StaticSocketDataProvider data1(&data1_reads[0], data1_reads.size(),
                                 &data1_writes[0], data1_writes.size());
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  std::vector<MockRead> data2_reads;
  std::vector<MockWrite> data2_writes;

  if (use_spdy) {
    data2_writes.push_back(CreateMockWrite(*spdy_request, 0, ASYNC));

    data2_reads.push_back(CreateMockRead(*spdy_response, 1, ASYNC));
    data2_reads.push_back(CreateMockRead(*spdy_data, 2, ASYNC));
    data2_reads.push_back(MockRead(ASYNC, OK, 3));
  } else {
    data2_writes.push_back(
        MockWrite(ASYNC, kHttpRequest, strlen(kHttpRequest), 0));

    data2_reads.push_back(
        MockRead(ASYNC, kHttpResponse, strlen(kHttpResponse), 1));
    data2_reads.push_back(MockRead(ASYNC, kHttpData, strlen(kHttpData), 2));
    data2_reads.push_back(MockRead(ASYNC, OK, 3));
  }
  OrderedSocketData data2(&data2_reads[0], data2_reads.size(),
                          &data2_writes[0], data2_writes.size());
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  // Preconnect a socket.
  net::SSLConfig ssl_config;
  session->ssl_config_service()->GetSSLConfig(&ssl_config);
  session->GetNextProtos(&ssl_config.next_protos);
  session->http_stream_factory()->PreconnectStreams(
      1, request, DEFAULT_PRIORITY, ssl_config, ssl_config);
  // Wait for the preconnect to complete.
  // TODO(davidben): Some way to wait for an idle socket count might be handy.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetIdleSocketCountInSSLSocketPool(session.get()));

  // Make the request.
  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(
      load_timing_info,
      CONNECT_TIMING_HAS_DNS_TIMES|CONNECT_TIMING_HAS_SSL_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(kHttpData, response_data);
}

TEST_P(HttpNetworkTransactionTest,
       KeepAliveConnectionNotConnectedOnWrite) {
  MockWrite write_failure(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  KeepAliveConnectionResendRequestTest(&write_failure, NULL);
}

TEST_P(HttpNetworkTransactionTest, KeepAliveConnectionReset) {
  MockRead read_failure(ASYNC, ERR_CONNECTION_RESET);
  KeepAliveConnectionResendRequestTest(NULL, &read_failure);
}

TEST_P(HttpNetworkTransactionTest, KeepAliveConnectionEOF) {
  MockRead read_failure(SYNCHRONOUS, OK);  // EOF
  KeepAliveConnectionResendRequestTest(NULL, &read_failure);
}

// Make sure that on a 408 response (Request Timeout), the request is retried,
// if the socket was a reused keep alive socket.
TEST_P(HttpNetworkTransactionTest, KeepAlive408) {
  MockRead read_failure(SYNCHRONOUS,
                        "HTTP/1.1 408 Request Timeout\r\n"
                        "Connection: Keep-Alive\r\n"
                        "Content-Length: 6\r\n\r\n"
                        "Pickle");
  KeepAliveConnectionResendRequestTest(NULL, &read_failure);
}

TEST_P(HttpNetworkTransactionTest,
       PreconnectErrorNotConnectedOnWrite) {
  MockWrite write_failure(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  PreconnectErrorResendRequestTest(&write_failure, NULL, false);
}

TEST_P(HttpNetworkTransactionTest, PreconnectErrorReset) {
  MockRead read_failure(ASYNC, ERR_CONNECTION_RESET);
  PreconnectErrorResendRequestTest(NULL, &read_failure, false);
}

TEST_P(HttpNetworkTransactionTest, PreconnectErrorEOF) {
  MockRead read_failure(SYNCHRONOUS, OK);  // EOF
  PreconnectErrorResendRequestTest(NULL, &read_failure, false);
}

TEST_P(HttpNetworkTransactionTest, PreconnectErrorAsyncEOF) {
  MockRead read_failure(ASYNC, OK);  // EOF
  PreconnectErrorResendRequestTest(NULL, &read_failure, false);
}

// Make sure that on a 408 response (Request Timeout), the request is retried,
// if the socket was a preconnected (UNUSED_IDLE) socket.
TEST_P(HttpNetworkTransactionTest, RetryOnIdle408) {
  MockRead read_failure(SYNCHRONOUS,
                        "HTTP/1.1 408 Request Timeout\r\n"
                        "Connection: Keep-Alive\r\n"
                        "Content-Length: 6\r\n\r\n"
                        "Pickle");
  KeepAliveConnectionResendRequestTest(NULL, &read_failure);
  PreconnectErrorResendRequestTest(NULL, &read_failure, false);
}

TEST_P(HttpNetworkTransactionTest,
       SpdyPreconnectErrorNotConnectedOnWrite) {
  MockWrite write_failure(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  PreconnectErrorResendRequestTest(&write_failure, NULL, true);
}

TEST_P(HttpNetworkTransactionTest, SpdyPreconnectErrorReset) {
  MockRead read_failure(ASYNC, ERR_CONNECTION_RESET);
  PreconnectErrorResendRequestTest(NULL, &read_failure, true);
}

TEST_P(HttpNetworkTransactionTest, SpdyPreconnectErrorEOF) {
  MockRead read_failure(SYNCHRONOUS, OK);  // EOF
  PreconnectErrorResendRequestTest(NULL, &read_failure, true);
}

TEST_P(HttpNetworkTransactionTest, SpdyPreconnectErrorAsyncEOF) {
  MockRead read_failure(ASYNC, OK);  // EOF
  PreconnectErrorResendRequestTest(NULL, &read_failure, true);
}

TEST_P(HttpNetworkTransactionTest, NonKeepAliveConnectionReset) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockRead data_reads[] = {
    MockRead(ASYNC, ERR_CONNECTION_RESET),
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

// What do various browsers do when the server closes a non-keepalive
// connection without sending any response header or body?
//
// IE7: error page
// Safari 3.1.2 (Windows): error page
// Firefox 3.0.1: blank page
// Opera 9.52: after five attempts, blank page
// Us with WinHTTP: error page (ERR_INVALID_RESPONSE)
// Us: error page (EMPTY_RESPONSE)
TEST_P(HttpNetworkTransactionTest, NonKeepAliveConnectionEOF) {
  MockRead data_reads[] = {
    MockRead(SYNCHRONOUS, OK),  // EOF
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads,
                                              arraysize(data_reads));
  EXPECT_EQ(ERR_EMPTY_RESPONSE, out.rv);
}

// Test that network access can be deferred and resumed.
TEST_P(HttpNetworkTransactionTest, ThrottleBeforeNetworkStart) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Defer on OnBeforeNetworkStart.
  BeforeNetworkStartHandler net_start_handler(true);  // defer
  trans->SetBeforeNetworkStartCallback(
      base::Bind(&BeforeNetworkStartHandler::OnBeforeNetworkStart,
                 base::Unretained(&net_start_handler)));

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("hello"),
    MockRead(SYNCHRONOUS, 0),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  base::MessageLoop::current()->RunUntilIdle();

  // Should have deferred for network start.
  EXPECT_TRUE(net_start_handler.observed_before_network_start());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_DELEGATE, trans->GetLoadState());
  EXPECT_TRUE(trans->GetResponseInfo() == NULL);

  trans->ResumeNetworkStart();
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(trans->GetResponseInfo() != NULL);

  scoped_refptr<IOBufferWithSize> io_buf(new IOBufferWithSize(100));
  rv = trans->Read(io_buf.get(), io_buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(5, rv);
  trans.reset();
}

// Test that network use can be deferred and canceled.
TEST_P(HttpNetworkTransactionTest, ThrottleAndCancelBeforeNetworkStart) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Defer on OnBeforeNetworkStart.
  BeforeNetworkStartHandler net_start_handler(true);  // defer
  trans->SetBeforeNetworkStartCallback(
      base::Bind(&BeforeNetworkStartHandler::OnBeforeNetworkStart,
                 base::Unretained(&net_start_handler)));

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  base::MessageLoop::current()->RunUntilIdle();

  // Should have deferred for network start.
  EXPECT_TRUE(net_start_handler.observed_before_network_start());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_DELEGATE, trans->GetLoadState());
  EXPECT_TRUE(trans->GetResponseInfo() == NULL);
}

// Next 2 cases (KeepAliveEarlyClose and KeepAliveEarlyClose2) are regression
// tests. There was a bug causing HttpNetworkTransaction to hang in the
// destructor in such situations.
// See http://crbug.com/154712 and http://crbug.com/156609.
TEST_P(HttpNetworkTransactionTest, KeepAliveEarlyClose) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Connection: keep-alive\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead("hello"),
    MockRead(SYNCHRONOUS, 0),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_refptr<IOBufferWithSize> io_buf(new IOBufferWithSize(100));
  rv = trans->Read(io_buf.get(), io_buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(5, rv);
  rv = trans->Read(io_buf.get(), io_buf->size(), callback.callback());
  EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, rv);

  trans.reset();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));
}

TEST_P(HttpNetworkTransactionTest, KeepAliveEarlyClose2) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Connection: keep-alive\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, 0),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_refptr<IOBufferWithSize> io_buf(new IOBufferWithSize(100));
  rv = trans->Read(io_buf.get(), io_buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, rv);

  trans.reset();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));
}

// Test that we correctly reuse a keep-alive connection after not explicitly
// reading the body.
TEST_P(HttpNetworkTransactionTest, KeepAliveAfterUnreadBody) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Note that because all these reads happen in the same
  // StaticSocketDataProvider, it shows that the same socket is being reused for
  // all transactions.
  MockRead data1_reads[] = {
    MockRead("HTTP/1.1 204 No Content\r\n\r\n"),
    MockRead("HTTP/1.1 205 Reset Content\r\n\r\n"),
    MockRead("HTTP/1.1 304 Not Modified\r\n\r\n"),
    MockRead("HTTP/1.1 302 Found\r\n"
             "Content-Length: 0\r\n\r\n"),
    MockRead("HTTP/1.1 302 Found\r\n"
             "Content-Length: 5\r\n\r\n"
             "hello"),
    MockRead("HTTP/1.1 301 Moved Permanently\r\n"
             "Content-Length: 0\r\n\r\n"),
    MockRead("HTTP/1.1 301 Moved Permanently\r\n"
             "Content-Length: 5\r\n\r\n"
             "hello"),
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
  };
  StaticSocketDataProvider data1(data1_reads, arraysize(data1_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  MockRead data2_reads[] = {
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };
  StaticSocketDataProvider data2(data2_reads, arraysize(data2_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  const int kNumUnreadBodies = arraysize(data1_reads) - 2;
  std::string response_lines[kNumUnreadBodies];

  uint32 first_socket_log_id = NetLog::Source::kInvalidId;
  for (size_t i = 0; i < arraysize(data1_reads) - 2; ++i) {
    TestCompletionCallback callback;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    int rv = trans->Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    LoadTimingInfo load_timing_info;
    EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
    if (i == 0) {
      TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
      first_socket_log_id = load_timing_info.socket_log_id;
    } else {
      TestLoadTimingReused(load_timing_info);
      EXPECT_EQ(first_socket_log_id, load_timing_info.socket_log_id);
    }

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);

    ASSERT_TRUE(response->headers.get() != NULL);
    response_lines[i] = response->headers->GetStatusLine();

    // We intentionally don't read the response bodies.
  }

  const char* const kStatusLines[] = {
    "HTTP/1.1 204 No Content",
    "HTTP/1.1 205 Reset Content",
    "HTTP/1.1 304 Not Modified",
    "HTTP/1.1 302 Found",
    "HTTP/1.1 302 Found",
    "HTTP/1.1 301 Moved Permanently",
    "HTTP/1.1 301 Moved Permanently",
  };

  COMPILE_ASSERT(kNumUnreadBodies == arraysize(kStatusLines),
                 forgot_to_update_kStatusLines);

  for (int i = 0; i < kNumUnreadBodies; ++i)
    EXPECT_EQ(kStatusLines[i], response_lines[i]);

  TestCompletionCallback callback;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello", response_data);
}

// Test the request-challenge-retry sequence for basic auth.
// (basic auth is the easiest to mock, because it has no randomness).
TEST_P(HttpNetworkTransactionTest, BasicAuth) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  CapturingNetLog log;
  session_deps_.net_log = &log;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    // Give a couple authenticate options (only the middle one is actually
    // supported).
    MockRead("WWW-Authenticate: Basic invalid\r\n"),  // Malformed.
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("WWW-Authenticate: UNSUPPORTED realm=\"FOO\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // Large content-length -- won't matter, as connection will be reset.
    MockRead("Content-Length: 10000\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  // After calling trans->RestartWithAuth(), this is the request we should
  // be issuing -- the final header line contains the credentials.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info1;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info1));
  TestLoadTimingNotReused(load_timing_info1, CONNECT_TIMING_HAS_DNS_TIMES);

  int64 reads_size1 = ReadsSize(data_reads1, arraysize(data_reads1));
  EXPECT_EQ(reads_size1, trans->GetTotalReceivedBytes());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingNotReused(load_timing_info2, CONNECT_TIMING_HAS_DNS_TIMES);
  // The load timing after restart should have a new socket ID, and times after
  // those of the first load timing.
  EXPECT_LE(load_timing_info1.receive_headers_end,
            load_timing_info2.connect_timing.connect_start);
  EXPECT_NE(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  int64 reads_size2 = ReadsSize(data_reads2, arraysize(data_reads2));
  EXPECT_EQ(reads_size1 + reads_size2, trans->GetTotalReceivedBytes());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

TEST_P(HttpNetworkTransactionTest, DoNotSendAuth) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // Large content-length -- won't matter, as connection will be reset.
    MockRead("Content-Length: 10000\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  int64 reads_size = ReadsSize(data_reads, arraysize(data_reads));
  EXPECT_EQ(reads_size, trans->GetTotalReceivedBytes());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection.
TEST_P(HttpNetworkTransactionTest, BasicAuthKeepAlive) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  CapturingNetLog log;
  session_deps_.net_log = &log;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 14\r\n\r\n"),
    MockRead("Unauthorized\r\n"),

    // Lastly, the server responds with the actual content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("Hello"),
  };

  // If there is a regression where we disconnect a Keep-Alive
  // connection during an auth roundtrip, we'll end up reading this.
  MockRead data_reads2[] = {
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info1;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info1));
  TestLoadTimingNotReused(load_timing_info1, CONNECT_TIMING_HAS_DNS_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingReused(load_timing_info2);
  // The load timing after restart should have the same socket ID, and times
  // those of the first load timing.
  EXPECT_LE(load_timing_info1.receive_headers_end,
            load_timing_info2.send_start);
  EXPECT_EQ(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(5, response->headers->GetContentLength());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  int64 reads_size1 = ReadsSize(data_reads1, arraysize(data_reads1));
  EXPECT_EQ(reads_size1, trans->GetTotalReceivedBytes());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection and with no response body to drain.
TEST_P(HttpNetworkTransactionTest, BasicAuthKeepAliveNoBody) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),  // No response body.

    // Lastly, the server responds with the actual content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("hello"),
  };

  // An incorrect reconnect would cause this to be read.
  MockRead data_reads2[] = {
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(5, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection and with a large response body to drain.
TEST_P(HttpNetworkTransactionTest, BasicAuthKeepAliveLargeBody) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Respond with 5 kb of response body.
  std::string large_body_string("Unauthorized");
  large_body_string.append(5 * 1024, ' ');
  large_body_string.append("\r\n");

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // 5134 = 12 + 5 * 1024 + 2
    MockRead("Content-Length: 5134\r\n\r\n"),
    MockRead(ASYNC, large_body_string.data(), large_body_string.size()),

    // Lastly, the server responds with the actual content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("hello"),
  };

  // An incorrect reconnect would cause this to be read.
  MockRead data_reads2[] = {
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(5, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection, but the server gets impatient and closes the connection.
TEST_P(HttpNetworkTransactionTest, BasicAuthKeepAliveImpatientServer) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
    // This simulates the seemingly successful write to a closed connection
    // if the bug is not fixed.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 14\r\n\r\n"),
    // Tell MockTCPClientSocket to simulate the server closing the connection.
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead("Unauthorized\r\n"),
    MockRead(SYNCHRONOUS, OK),  // The server closes the connection.
  };

  // After calling trans->RestartWithAuth(), this is the request we should
  // be issuing -- the final header line contains the credentials.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead("hello"),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(5, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a connection
// that requires a restart when setting up an SSL tunnel.
TEST_P(HttpNetworkTransactionTest, BasicAuthProxyNoKeepAlive) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  // when the no authentication data flag is set.
  request.load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),

    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  MockRead data_reads1[] = {
    // No credentials.
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Proxy-Connection: close\r\n\r\n"),

    MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 5\r\n\r\n"),
    MockRead(SYNCHRONOUS, "hello"),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLog::PHASE_NONE);
  ExpectLogContainsSomewhere(
      entries, pos,
      NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLog::PHASE_NONE);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_FALSE(response->headers.get() == NULL);
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  LoadTimingInfo load_timing_info;
  // CONNECT requests and responses are handled at the connect job level, so
  // the transaction does not yet have a connection.
  EXPECT_FALSE(trans->GetLoadTimingInfo(&load_timing_info));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(5, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should not be set.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);

  trans.reset();
  session->CloseAllConnections();
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// proxy connection, when setting up an SSL tunnel.
TEST_P(HttpNetworkTransactionTest, BasicAuthProxyKeepAlive) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  // Ensure that proxy authentication is attempted even
  // when the no authentication data flag is set.
  request.load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  MockRead data_reads1[] = {
    // No credentials.
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead("0123456789"),

    // Wrong credentials (wrong password).
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    // No response body because the test stops reading here.
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLog::PHASE_NONE);
  ExpectLogContainsSomewhere(
      entries, pos,
      NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLog::PHASE_NONE);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_FALSE(response->headers.get() == NULL);
  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  // Wrong password (should be "bar").
  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBaz), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_FALSE(response->headers.get() == NULL);
  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  // Flush the idle socket before the NetLog and HttpNetworkTransaction go
  // out of scope.
  session->CloseAllConnections();
}

// Test that we don't read the response body when we fail to establish a tunnel,
// even if the user cancels the proxy's auth attempt.
TEST_P(HttpNetworkTransactionTest, BasicAuthProxyCancelTunnel) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407.
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);

  // Flush the idle socket before the HttpNetworkTransaction goes out of scope.
  session->CloseAllConnections();
}

// Test when a server (non-proxy) returns a 407 (proxy-authenticate).
// The request should fail with ERR_UNEXPECTED_PROXY_AUTH.
TEST_P(HttpNetworkTransactionTest, UnexpectedProxyAuth) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // We are using a DIRECT connection (i.e. no proxy) for this session.
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 407 Proxy Auth required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    // Large content-length -- won't matter, as connection will be reset.
    MockRead("Content-Length: 10000\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_UNEXPECTED_PROXY_AUTH, rv);
}

// Tests when an HTTPS server (non-proxy) returns a 407 (proxy-authentication)
// through a non-authenticating proxy. The request should fail with
// ERR_UNEXPECTED_PROXY_AUTH.
// Note that it is impossible to detect if an HTTP server returns a 407 through
// a non-authenticating proxy - there is nothing to indicate whether the
// response came from the proxy or the server, so it is treated as if the proxy
// issued the challenge.
TEST_P(HttpNetworkTransactionTest,
       HttpsServerRequestsProxyAuthThroughProxy) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");

  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),

    MockRead("HTTP/1.1 407 Unauthorized\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(ERR_UNEXPECTED_PROXY_AUTH, rv);
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLog::PHASE_NONE);
  ExpectLogContainsSomewhere(
      entries, pos,
      NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLog::PHASE_NONE);
}

// Test the load timing for HTTPS requests with an HTTP proxy.
TEST_P(HttpNetworkTransactionTest, HttpProxyLoadTimingNoPacTwoRequests) {
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/1");

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.google.com/2");

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("PROXY myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    MockWrite("GET /1 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    MockWrite("GET /2 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 1\r\n\r\n"),
    MockRead(SYNCHRONOUS, "1"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 2\r\n\r\n"),
    MockRead(SYNCHRONOUS, "22"),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;
  scoped_ptr<HttpTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans1->Start(&request1, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);
  ASSERT_TRUE(response1->headers.get() != NULL);
  EXPECT_EQ(1, response1->headers->GetContentLength());

  LoadTimingInfo load_timing_info1;
  EXPECT_TRUE(trans1->GetLoadTimingInfo(&load_timing_info1));
  TestLoadTimingNotReused(load_timing_info1, CONNECT_TIMING_HAS_SSL_TIMES);

  trans1.reset();

  TestCompletionCallback callback2;
  scoped_ptr<HttpTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans2->Start(&request2, callback2.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);
  ASSERT_TRUE(response2->headers.get() != NULL);
  EXPECT_EQ(2, response2->headers->GetContentLength());

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans2->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingReused(load_timing_info2);

  EXPECT_EQ(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  trans2.reset();
  session->CloseAllConnections();
}

// Test the load timing for HTTPS requests with an HTTP proxy and a PAC script.
TEST_P(HttpNetworkTransactionTest, HttpProxyLoadTimingWithPacTwoRequests) {
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/1");

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.google.com/2");

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    MockWrite("GET /1 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    MockWrite("GET /2 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 1\r\n\r\n"),
    MockRead(SYNCHRONOUS, "1"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 2\r\n\r\n"),
    MockRead(SYNCHRONOUS, "22"),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;
  scoped_ptr<HttpTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans1->Start(&request1, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);
  ASSERT_TRUE(response1->headers.get() != NULL);
  EXPECT_EQ(1, response1->headers->GetContentLength());

  LoadTimingInfo load_timing_info1;
  EXPECT_TRUE(trans1->GetLoadTimingInfo(&load_timing_info1));
  TestLoadTimingNotReusedWithPac(load_timing_info1,
                                 CONNECT_TIMING_HAS_SSL_TIMES);

  trans1.reset();

  TestCompletionCallback callback2;
  scoped_ptr<HttpTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans2->Start(&request2, callback2.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);
  ASSERT_TRUE(response2->headers.get() != NULL);
  EXPECT_EQ(2, response2->headers->GetContentLength());

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans2->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingReusedWithPac(load_timing_info2);

  EXPECT_EQ(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  trans2.reset();
  session->CloseAllConnections();
}

// Test a simple get through an HTTPS Proxy.
TEST_P(HttpNetworkTransactionTest, HttpsProxyGet) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");

  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should use full url
  MockWrite data_writes1[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info,
                          CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(100, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should not be set.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
}

// Test a SPDY get through an HTTPS Proxy.
TEST_P(HttpNetworkTransactionTest, HttpsProxySpdyGet) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // fetch http://www.google.com/ via SPDY
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, false));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*data),
    MockRead(ASYNC, 0, 0),
  };

  DelayedSocketData spdy_data(
      1,  // wait for one write to finish before reading.
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info,
                          CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ(kUploadData, response_data);
}

// Verifies that a session which races and wins against the owning transaction
// (completing prior to host resolution), doesn't fail the transaction.
// Regression test for crbug.com/334413.
TEST_P(HttpNetworkTransactionTest, HttpsProxySpdyGetWithSessionRace) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // Configure SPDY proxy server "proxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Fetch http://www.google.com/ through the SPDY proxy.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, false));
  MockWrite spdy_writes[] = {CreateMockWrite(*req)};

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
      CreateMockRead(*resp), CreateMockRead(*data), MockRead(ASYNC, 0, 0),
  };

  DelayedSocketData spdy_data(
      1,  // wait for one write to finish before reading.
      spdy_reads,
      arraysize(spdy_reads),
      spdy_writes,
      arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Stall the hostname resolution begun by the transaction.
  session_deps_.host_resolver->set_synchronous_mode(false);
  session_deps_.host_resolver->set_ondemand_mode(true);

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Race a session to the proxy, which completes first.
  session_deps_.host_resolver->set_ondemand_mode(false);
  SpdySessionKey key(
      HostPortPair("proxy", 70), ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> spdy_session =
      CreateSecureSpdySession(session, key, log.bound());

  // Unstall the resolution begun by the transaction.
  session_deps_.host_resolver->set_ondemand_mode(true);
  session_deps_.host_resolver->ResolveAllPending();

  EXPECT_FALSE(callback1.have_result());
  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ(kUploadData, response_data);
}

// Test a SPDY get through an HTTPS Proxy.
TEST_P(HttpNetworkTransactionTest, HttpsProxySpdyGetWithProxyAuth) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // Configure against https proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // The first request will be a bare GET, the second request will be a
  // GET with a Proxy-Authorization header.
  scoped_ptr<SpdyFrame> req_get(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, false));
  const char* const kExtraAuthorizationHeaders[] = {
    "proxy-authorization", "Basic Zm9vOmJhcg=="
  };
  scoped_ptr<SpdyFrame> req_get_authorization(
      spdy_util_.ConstructSpdyGet(kExtraAuthorizationHeaders,
                                  arraysize(kExtraAuthorizationHeaders) / 2,
                                  false,
                                  3,
                                  LOWEST,
                                  false));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req_get, 1),
    CreateMockWrite(*req_get_authorization, 4),
  };

  // The first response is a 407 proxy authentication challenge, and the second
  // response will be a 200 response since the second request includes a valid
  // Authorization header.
  const char* const kExtraAuthenticationHeaders[] = {
    "proxy-authenticate", "Basic realm=\"MyRealm1\""
  };
  scoped_ptr<SpdyFrame> resp_authentication(
      spdy_util_.ConstructSpdySynReplyError(
          "407 Proxy Authentication Required",
          kExtraAuthenticationHeaders, arraysize(kExtraAuthenticationHeaders)/2,
          1));
  scoped_ptr<SpdyFrame> body_authentication(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> resp_data(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body_data(spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp_authentication, 2),
    CreateMockRead(*body_authentication, 3),
    CreateMockRead(*resp_data, 5),
    CreateMockRead(*body_data, 6),
    MockRead(ASYNC, 0, 7),
  };

  OrderedSocketData data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* const response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* const response_restart = trans->GetResponseInfo();

  ASSERT_TRUE(response_restart != NULL);
  ASSERT_TRUE(response_restart->headers.get() != NULL);
  EXPECT_EQ(200, response_restart->headers->response_code());
  // The password prompt info should not be set.
  EXPECT_TRUE(response_restart->auth_challenge.get() == NULL);
}

// Test a SPDY CONNECT through an HTTPS Proxy to an HTTPS (non-SPDY) Server.
TEST_P(HttpNetworkTransactionTest, HttpsProxySpdyConnectHttps) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // CONNECT to www.google.com:443 via SPDY
  scoped_ptr<SpdyFrame> connect(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                                LOWEST));
  // fetch https://www.google.com/ via HTTP

  const char get[] = "GET / HTTP/1.1\r\n"
    "Host: www.google.com\r\n"
    "Connection: keep-alive\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get(
      spdy_util_.ConstructSpdyBodyFrame(1, get, strlen(get), false));
  scoped_ptr<SpdyFrame> conn_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  const char resp[] = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get_resp(
      spdy_util_.ConstructSpdyBodyFrame(1, resp, strlen(resp), false));
  scoped_ptr<SpdyFrame> wrapped_body(
      spdy_util_.ConstructSpdyBodyFrame(1, "1234567890", 10, false));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, wrapped_get_resp->size()));

  MockWrite spdy_writes[] = {
      CreateMockWrite(*connect, 1),
      CreateMockWrite(*wrapped_get, 3),
      CreateMockWrite(*window_update, 5),
  };

  MockRead spdy_reads[] = {
    CreateMockRead(*conn_resp, 2, ASYNC),
    CreateMockRead(*wrapped_get_resp, 4, ASYNC),
    CreateMockRead(*wrapped_body, 6, ASYNC),
    CreateMockRead(*wrapped_body, 7, ASYNC),
    MockRead(ASYNC, 0, 8),
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.was_npn_negotiated = false;
  ssl2.protocol_negotiated = kProtoUnknown;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_SSL_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("1234567890", response_data);
}

// Test a SPDY CONNECT through an HTTPS Proxy to a SPDY server.
TEST_P(HttpNetworkTransactionTest, HttpsProxySpdyConnectSpdy) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // CONNECT to www.google.com:443 via SPDY
  scoped_ptr<SpdyFrame> connect(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                                LOWEST));
  // fetch https://www.google.com/ via SPDY
  const char* const kMyUrl = "https://www.google.com/";
  scoped_ptr<SpdyFrame> get(
      spdy_util_.ConstructSpdyGet(kMyUrl, false, 1, LOWEST));
  scoped_ptr<SpdyFrame> wrapped_get(
      spdy_util_.ConstructWrappedSpdyFrame(get, 1));
  scoped_ptr<SpdyFrame> conn_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> get_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> wrapped_get_resp(
      spdy_util_.ConstructWrappedSpdyFrame(get_resp, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> wrapped_body(
      spdy_util_.ConstructWrappedSpdyFrame(body, 1));
  scoped_ptr<SpdyFrame> window_update_get_resp(
      spdy_util_.ConstructSpdyWindowUpdate(1, wrapped_get_resp->size()));
  scoped_ptr<SpdyFrame> window_update_body(
      spdy_util_.ConstructSpdyWindowUpdate(1, wrapped_body->size()));

  MockWrite spdy_writes[] = {
      CreateMockWrite(*connect, 1),
      CreateMockWrite(*wrapped_get, 3),
      CreateMockWrite(*window_update_get_resp, 5),
      CreateMockWrite(*window_update_body, 7),
  };

  MockRead spdy_reads[] = {
    CreateMockRead(*conn_resp, 2, ASYNC),
    CreateMockRead(*wrapped_get_resp, 4, ASYNC),
    CreateMockRead(*wrapped_body, 6, ASYNC),
    MockRead(ASYNC, 0, 8),
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.SetNextProto(GetParam());
  ssl2.protocol_negotiated = GetParam();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_SSL_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ(kUploadData, response_data);
}

// Test a SPDY CONNECT failure through an HTTPS Proxy.
TEST_P(HttpNetworkTransactionTest, HttpsProxySpdyConnectFailure) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // CONNECT to www.google.com:443 via SPDY
  scoped_ptr<SpdyFrame> connect(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                                LOWEST));
  scoped_ptr<SpdyFrame> get(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));

  MockWrite spdy_writes[] = {
      CreateMockWrite(*connect, 1),
      CreateMockWrite(*get, 3),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdySynReplyError(1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 2, ASYNC),
    MockRead(ASYNC, 0, 4),
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);

  // TODO(ttuttle): Anything else to check here?
}

// Test load timing in the case of two HTTPS (non-SPDY) requests through a SPDY
// HTTPS Proxy to different servers.
TEST_P(HttpNetworkTransactionTest,
       HttpsProxySpdyConnectHttpsLoadTimingTwoRequestsTwoServers) {
  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_));

  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/");
  request1.load_flags = 0;

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://news.google.com/");
  request2.load_flags = 0;

  // CONNECT to www.google.com:443 via SPDY.
  scoped_ptr<SpdyFrame> connect1(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                                 LOWEST));
  scoped_ptr<SpdyFrame> conn_resp1(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  // Fetch https://www.google.com/ via HTTP.
  const char get1[] = "GET / HTTP/1.1\r\n"
      "Host: www.google.com\r\n"
      "Connection: keep-alive\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get1(
      spdy_util_.ConstructSpdyBodyFrame(1, get1, strlen(get1), false));
  const char resp1[] = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 1\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get_resp1(
      spdy_util_.ConstructSpdyBodyFrame(1, resp1, strlen(resp1), false));
  scoped_ptr<SpdyFrame> wrapped_body1(
      spdy_util_.ConstructSpdyBodyFrame(1, "1", 1, false));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, wrapped_get_resp1->size()));

  // CONNECT to news.google.com:443 via SPDY.
  SpdySynStreamIR connect2_ir(3);
  spdy_util_.SetPriority(LOWEST, &connect2_ir);
  connect2_ir.SetHeader(spdy_util_.GetMethodKey(), "CONNECT");
  connect2_ir.SetHeader(spdy_util_.GetPathKey(), "news.google.com:443");
  connect2_ir.SetHeader(spdy_util_.GetHostKey(), "news.google.com");
  spdy_util_.MaybeAddVersionHeader(&connect2_ir);
  scoped_ptr<SpdyFrame> connect2(
      spdy_util_.CreateFramer(false)->SerializeFrame(connect2_ir));

  scoped_ptr<SpdyFrame> conn_resp2(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));

  // Fetch https://news.google.com/ via HTTP.
  const char get2[] = "GET / HTTP/1.1\r\n"
      "Host: news.google.com\r\n"
      "Connection: keep-alive\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get2(
      spdy_util_.ConstructSpdyBodyFrame(3, get2, strlen(get2), false));
  const char resp2[] = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 2\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get_resp2(
      spdy_util_.ConstructSpdyBodyFrame(3, resp2, strlen(resp2), false));
  scoped_ptr<SpdyFrame> wrapped_body2(
      spdy_util_.ConstructSpdyBodyFrame(3, "22", 2, false));

  MockWrite spdy_writes[] = {
      CreateMockWrite(*connect1, 0),
      CreateMockWrite(*wrapped_get1, 2),
      CreateMockWrite(*connect2, 5),
      CreateMockWrite(*wrapped_get2, 7),
  };

  MockRead spdy_reads[] = {
    CreateMockRead(*conn_resp1, 1, ASYNC),
    CreateMockRead(*wrapped_get_resp1, 3, ASYNC),
    CreateMockRead(*wrapped_body1, 4, ASYNC),
    CreateMockRead(*conn_resp2, 6, ASYNC),
    CreateMockRead(*wrapped_get_resp2, 8, ASYNC),
    CreateMockRead(*wrapped_body2, 9, ASYNC),
    MockRead(ASYNC, 0, 10),
  };

  DeterministicSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.was_npn_negotiated = false;
  ssl2.protocol_negotiated = kProtoUnknown;
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl2);
  SSLSocketDataProvider ssl3(ASYNC, OK);
  ssl3.was_npn_negotiated = false;
  ssl3.protocol_negotiated = kProtoUnknown;
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl3);

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // The first connect and request, each of their responses, and the body.
  spdy_data.RunFor(5);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_SSL_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
  EXPECT_EQ(1, trans->Read(buf.get(), 256, callback.callback()));

  scoped_ptr<HttpTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  rv = trans2->Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The second connect and request, each of their responses, and the body.
  spdy_data.RunFor(5);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans2->GetLoadTimingInfo(&load_timing_info2));
  // Even though the SPDY connection is reused, a new tunnelled connection has
  // to be created, so the socket's load timing looks like a fresh connection.
  TestLoadTimingNotReused(load_timing_info2, CONNECT_TIMING_HAS_SSL_TIMES);

  // The requests should have different IDs, since they each are using their own
  // separate stream.
  EXPECT_NE(load_timing_info.socket_log_id, load_timing_info2.socket_log_id);

  EXPECT_EQ(2, trans2->Read(buf.get(), 256, callback.callback()));
}

// Test load timing in the case of two HTTPS (non-SPDY) requests through a SPDY
// HTTPS Proxy to the same server.
TEST_P(HttpNetworkTransactionTest,
       HttpsProxySpdyConnectHttpsLoadTimingTwoRequestsSameServer) {
  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_));

  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/");
  request1.load_flags = 0;

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.google.com/2");
  request2.load_flags = 0;

  // CONNECT to www.google.com:443 via SPDY.
  scoped_ptr<SpdyFrame> connect1(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                                 LOWEST));
  scoped_ptr<SpdyFrame> conn_resp1(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  // Fetch https://www.google.com/ via HTTP.
  const char get1[] = "GET / HTTP/1.1\r\n"
      "Host: www.google.com\r\n"
      "Connection: keep-alive\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get1(
      spdy_util_.ConstructSpdyBodyFrame(1, get1, strlen(get1), false));
  const char resp1[] = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 1\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get_resp1(
      spdy_util_.ConstructSpdyBodyFrame(1, resp1, strlen(resp1), false));
  scoped_ptr<SpdyFrame> wrapped_body1(
      spdy_util_.ConstructSpdyBodyFrame(1, "1", 1, false));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, wrapped_get_resp1->size()));

  // Fetch https://www.google.com/2 via HTTP.
  const char get2[] = "GET /2 HTTP/1.1\r\n"
      "Host: www.google.com\r\n"
      "Connection: keep-alive\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get2(
      spdy_util_.ConstructSpdyBodyFrame(1, get2, strlen(get2), false));
  const char resp2[] = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 2\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get_resp2(
      spdy_util_.ConstructSpdyBodyFrame(1, resp2, strlen(resp2), false));
  scoped_ptr<SpdyFrame> wrapped_body2(
      spdy_util_.ConstructSpdyBodyFrame(1, "22", 2, false));

  MockWrite spdy_writes[] = {
      CreateMockWrite(*connect1, 0),
      CreateMockWrite(*wrapped_get1, 2),
      CreateMockWrite(*wrapped_get2, 5),
  };

  MockRead spdy_reads[] = {
    CreateMockRead(*conn_resp1, 1, ASYNC),
    CreateMockRead(*wrapped_get_resp1, 3, ASYNC),
    CreateMockRead(*wrapped_body1, 4, ASYNC),
    CreateMockRead(*wrapped_get_resp2, 6, ASYNC),
    CreateMockRead(*wrapped_body2, 7, ASYNC),
    MockRead(ASYNC, 0, 8),
  };

  DeterministicSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.was_npn_negotiated = false;
  ssl2.protocol_negotiated = kProtoUnknown;
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl2);

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // The first connect and request, each of their responses, and the body.
  spdy_data.RunFor(5);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_SSL_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
  EXPECT_EQ(1, trans->Read(buf.get(), 256, callback.callback()));
  trans.reset();

  scoped_ptr<HttpTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  rv = trans2->Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The second request, response, and body.  There should not be a second
  // connect.
  spdy_data.RunFor(3);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans2->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingReused(load_timing_info2);

  // The requests should have the same ID.
  EXPECT_EQ(load_timing_info.socket_log_id, load_timing_info2.socket_log_id);

  EXPECT_EQ(2, trans2->Read(buf.get(), 256, callback.callback()));
}

// Test load timing in the case of of two HTTP requests through a SPDY HTTPS
// Proxy to different servers.
TEST_P(HttpNetworkTransactionTest,
       HttpsProxySpdyLoadTimingTwoHttpRequests) {
  // Configure against https proxy server "proxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_));

  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("http://www.google.com/");
  request1.load_flags = 0;

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("http://news.google.com/");
  request2.load_flags = 0;

  // http://www.google.com/
  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlockForProxy("http://www.google.com/"));
  scoped_ptr<SpdyFrame> get1(spdy_util_.ConstructSpdyControlFrame(
      headers.Pass(), false, 1, LOWEST, SYN_STREAM, CONTROL_FLAG_FIN, 0));
  scoped_ptr<SpdyFrame> get_resp1(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(1, "1", 1, true));

  // http://news.google.com/
  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlockForProxy("http://news.google.com/"));
  scoped_ptr<SpdyFrame> get2(spdy_util_.ConstructSpdyControlFrame(
      headers2.Pass(), false, 3, LOWEST, SYN_STREAM, CONTROL_FLAG_FIN, 0));
  scoped_ptr<SpdyFrame> get_resp2(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(3, "22", 2, true));

  MockWrite spdy_writes[] = {
      CreateMockWrite(*get1, 0),
      CreateMockWrite(*get2, 3),
  };

  MockRead spdy_reads[] = {
    CreateMockRead(*get_resp1, 1, ASYNC),
    CreateMockRead(*body1, 2, ASYNC),
    CreateMockRead(*get_resp2, 4, ASYNC),
    CreateMockRead(*body2, 5, ASYNC),
    MockRead(ASYNC, 0, 6),
  };

  DeterministicSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans->Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  spdy_data.RunFor(2);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info,
                          CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
  EXPECT_EQ(ERR_IO_PENDING, trans->Read(buf.get(), 256, callback.callback()));
  spdy_data.RunFor(1);
  EXPECT_EQ(1, callback.WaitForResult());
  // Delete the first request, so the second one can reuse the socket.
  trans.reset();

  scoped_ptr<HttpTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  rv = trans2->Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  spdy_data.RunFor(2);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans2->GetLoadTimingInfo(&load_timing_info2));
  TestLoadTimingReused(load_timing_info2);

  // The requests should have the same ID.
  EXPECT_EQ(load_timing_info.socket_log_id, load_timing_info2.socket_log_id);

  EXPECT_EQ(ERR_IO_PENDING, trans2->Read(buf.get(), 256, callback.callback()));
  spdy_data.RunFor(1);
  EXPECT_EQ(2, callback.WaitForResult());
}

// Test the challenge-response-retry sequence through an HTTPS Proxy
TEST_P(HttpNetworkTransactionTest, HttpsProxyAuthRetry) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  // when the no authentication data flag is set.
  request.load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // Configure against https proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should use full url
  MockWrite data_writes1[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // The proxy responds to the GET with a 407, using a persistent
  // connection.
  MockRead data_reads1[] = {
    // No credentials.
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Proxy-Connection: keep-alive\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info,
                          CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_FALSE(response->headers.get() == NULL);
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  load_timing_info = LoadTimingInfo();
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  // Retrying with HTTP AUTH is considered to be reusing a socket.
  TestLoadTimingReused(load_timing_info);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(100, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should not be set.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
}

void HttpNetworkTransactionTest::ConnectStatusHelperWithExpectedStatus(
    const MockRead& status, int expected_status) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    status,
    MockRead("Content-Length: 10\r\n\r\n"),
    // No response body because the test stops reading here.
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(expected_status, rv);
}

void HttpNetworkTransactionTest::ConnectStatusHelper(
    const MockRead& status) {
  ConnectStatusHelperWithExpectedStatus(
      status, ERR_TUNNEL_CONNECTION_FAILED);
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus100) {
  ConnectStatusHelper(MockRead("HTTP/1.1 100 Continue\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus101) {
  ConnectStatusHelper(MockRead("HTTP/1.1 101 Switching Protocols\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus201) {
  ConnectStatusHelper(MockRead("HTTP/1.1 201 Created\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus202) {
  ConnectStatusHelper(MockRead("HTTP/1.1 202 Accepted\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus203) {
  ConnectStatusHelper(
      MockRead("HTTP/1.1 203 Non-Authoritative Information\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus204) {
  ConnectStatusHelper(MockRead("HTTP/1.1 204 No Content\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus205) {
  ConnectStatusHelper(MockRead("HTTP/1.1 205 Reset Content\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus206) {
  ConnectStatusHelper(MockRead("HTTP/1.1 206 Partial Content\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus300) {
  ConnectStatusHelper(MockRead("HTTP/1.1 300 Multiple Choices\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus301) {
  ConnectStatusHelper(MockRead("HTTP/1.1 301 Moved Permanently\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus302) {
  ConnectStatusHelper(MockRead("HTTP/1.1 302 Found\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus303) {
  ConnectStatusHelper(MockRead("HTTP/1.1 303 See Other\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus304) {
  ConnectStatusHelper(MockRead("HTTP/1.1 304 Not Modified\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus305) {
  ConnectStatusHelper(MockRead("HTTP/1.1 305 Use Proxy\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus306) {
  ConnectStatusHelper(MockRead("HTTP/1.1 306\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus307) {
  ConnectStatusHelper(MockRead("HTTP/1.1 307 Temporary Redirect\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus308) {
  ConnectStatusHelper(MockRead("HTTP/1.1 308 Permanent Redirect\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus400) {
  ConnectStatusHelper(MockRead("HTTP/1.1 400 Bad Request\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus401) {
  ConnectStatusHelper(MockRead("HTTP/1.1 401 Unauthorized\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus402) {
  ConnectStatusHelper(MockRead("HTTP/1.1 402 Payment Required\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus403) {
  ConnectStatusHelper(MockRead("HTTP/1.1 403 Forbidden\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus404) {
  ConnectStatusHelper(MockRead("HTTP/1.1 404 Not Found\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus405) {
  ConnectStatusHelper(MockRead("HTTP/1.1 405 Method Not Allowed\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus406) {
  ConnectStatusHelper(MockRead("HTTP/1.1 406 Not Acceptable\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus407) {
  ConnectStatusHelperWithExpectedStatus(
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      ERR_PROXY_AUTH_UNSUPPORTED);
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus408) {
  ConnectStatusHelper(MockRead("HTTP/1.1 408 Request Timeout\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus409) {
  ConnectStatusHelper(MockRead("HTTP/1.1 409 Conflict\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus410) {
  ConnectStatusHelper(MockRead("HTTP/1.1 410 Gone\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus411) {
  ConnectStatusHelper(MockRead("HTTP/1.1 411 Length Required\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus412) {
  ConnectStatusHelper(MockRead("HTTP/1.1 412 Precondition Failed\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus413) {
  ConnectStatusHelper(MockRead("HTTP/1.1 413 Request Entity Too Large\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus414) {
  ConnectStatusHelper(MockRead("HTTP/1.1 414 Request-URI Too Long\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus415) {
  ConnectStatusHelper(MockRead("HTTP/1.1 415 Unsupported Media Type\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus416) {
  ConnectStatusHelper(
      MockRead("HTTP/1.1 416 Requested Range Not Satisfiable\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus417) {
  ConnectStatusHelper(MockRead("HTTP/1.1 417 Expectation Failed\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus500) {
  ConnectStatusHelper(MockRead("HTTP/1.1 500 Internal Server Error\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus501) {
  ConnectStatusHelper(MockRead("HTTP/1.1 501 Not Implemented\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus502) {
  ConnectStatusHelper(MockRead("HTTP/1.1 502 Bad Gateway\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus503) {
  ConnectStatusHelper(MockRead("HTTP/1.1 503 Service Unavailable\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus504) {
  ConnectStatusHelper(MockRead("HTTP/1.1 504 Gateway Timeout\r\n"));
}

TEST_P(HttpNetworkTransactionTest, ConnectStatus505) {
  ConnectStatusHelper(MockRead("HTTP/1.1 505 HTTP Version Not Supported\r\n"));
}

// Test the flow when both the proxy server AND origin server require
// authentication. Again, this uses basic auth for both since that is
// the simplest to mock.
TEST_P(HttpNetworkTransactionTest, BasicAuthProxyThenServer) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes1[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 407 Unauthorized\r\n"),
    // Give a couple authenticate options (only the middle one is actually
    // supported).
    MockRead("Proxy-Authenticate: Basic invalid\r\n"),  // Malformed.
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Proxy-Authenticate: UNSUPPORTED realm=\"FOO\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // Large content-length -- won't matter, as connection will be reset.
    MockRead("Content-Length: 10000\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  // After calling trans->RestartWithAuth() the first time, this is the
  // request we should be issuing -- the final header line contains the
  // proxy's credentials.
  MockWrite data_writes2[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Now the proxy server lets the request pass through to origin server.
  // The origin server responds with a 401.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    // Note: We are using the same realm-name as the proxy server. This is
    // completely valid, as realms are unique across hosts.
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 2000\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),  // Won't be reached.
  };

  // After calling trans->RestartWithAuth() the second time, we should send
  // the credentials for both the proxy and origin server.
  MockWrite data_writes3[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n"
              "Authorization: Basic Zm9vMjpiYXIy\r\n\r\n"),
  };

  // Lastly we get the desired content.
  MockRead data_reads3[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  StaticSocketDataProvider data3(data_reads3, arraysize(data_reads3),
                                 data_writes3, arraysize(data_writes3));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback3;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo2, kBar2), callback3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// For the NTLM implementation using SSPI, we skip the NTLM tests since we
// can't hook into its internals to cause it to generate predictable NTLM
// authorization headers.
#if defined(NTLM_PORTABLE)
// The NTLM authentication unit tests were generated by capturing the HTTP
// requests and responses using Fiddler 2 and inspecting the generated random
// bytes in the debugger.

// Enter the correct password and authenticate successfully.
TEST_P(HttpNetworkTransactionTest, NTLMAuth1) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://172.22.68.17/kids/login.aspx");

  // Ensure load is not disrupted by flags which suppress behaviour specific
  // to other auth schemes.
  request.load_flags = LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;

  HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGenerateRandom1,
                                                    MockGetHostName);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockWrite data_writes1[] = {
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    // Negotiate and NTLM are often requested together.  However, we only want
    // to test NTLM. Since Negotiate is preferred over NTLM, we have to skip
    // the header that requests Negotiate for this test.
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),
  };

  MockWrite data_writes2[] = {
    // After restarting with a null identity, this is the
    // request we should be issuing -- the final header line contains a Type
    // 1 message.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM "
              "TlRMTVNTUAABAAAAB4IIAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n"),

    // After calling trans->RestartWithAuth(), we should send a Type 3 message
    // (the credentials for the origin server).  The second request continues
    // on the same connection.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM TlRMTVNTUAADAAAAGAAYAGgAAAAYABgAgA"
              "AAAAAAAABAAAAAGAAYAEAAAAAQABAAWAAAAAAAAAAAAAAABYIIAHQA"
              "ZQBzAHQAaQBuAGcALQBuAHQAbABtAFcAVABDAC0AVwBJAE4ANwBVKW"
              "Yma5xzVAAAAAAAAAAAAAAAAAAAAACH+gWcm+YsP9Tqb9zCR3WAeZZX"
              "ahlhx5I=\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    // The origin server responds with a Type 2 message.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM "
             "TlRMTVNTUAACAAAADAAMADgAAAAFgokCjGpMpPGlYKkAAAAAAAAAALo"
             "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
             "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
             "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
             "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
             "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
             "BtAAAAAAA=\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Lastly we get the desired content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=utf-8\r\n"),
    MockRead("Content-Length: 13\r\n\r\n"),
    MockRead("Please Login\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_FALSE(response == NULL);
  EXPECT_TRUE(CheckNTLMServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(AuthCredentials(kTestingNTLM, kTestingNTLM),
                              callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  TestCompletionCallback callback3;

  rv = trans->RestartWithAuth(AuthCredentials(), callback3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(13, response->headers->GetContentLength());
}

// Enter a wrong password, and then the correct one.
TEST_P(HttpNetworkTransactionTest, NTLMAuth2) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://172.22.68.17/kids/login.aspx");
  request.load_flags = 0;

  HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGenerateRandom2,
                                                    MockGetHostName);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  MockWrite data_writes1[] = {
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    // Negotiate and NTLM are often requested together.  However, we only want
    // to test NTLM. Since Negotiate is preferred over NTLM, we have to skip
    // the header that requests Negotiate for this test.
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),
  };

  MockWrite data_writes2[] = {
    // After restarting with a null identity, this is the
    // request we should be issuing -- the final header line contains a Type
    // 1 message.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM "
              "TlRMTVNTUAABAAAAB4IIAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n"),

    // After calling trans->RestartWithAuth(), we should send a Type 3 message
    // (the credentials for the origin server).  The second request continues
    // on the same connection.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM TlRMTVNTUAADAAAAGAAYAGgAAAAYABgAgA"
              "AAAAAAAABAAAAAGAAYAEAAAAAQABAAWAAAAAAAAAAAAAAABYIIAHQA"
              "ZQBzAHQAaQBuAGcALQBuAHQAbABtAFcAVABDAC0AVwBJAE4ANwCWeY"
              "XnSZNwoQAAAAAAAAAAAAAAAAAAAADLa34/phTTKzNTWdub+uyFleOj"
              "4Ww7b7E=\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    // The origin server responds with a Type 2 message.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM "
             "TlRMTVNTUAACAAAADAAMADgAAAAFgokCbVWUZezVGpAAAAAAAAAAALo"
             "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
             "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
             "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
             "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
             "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
             "BtAAAAAAA=\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Wrong password.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),
  };

  MockWrite data_writes3[] = {
    // After restarting with a null identity, this is the
    // request we should be issuing -- the final header line contains a Type
    // 1 message.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM "
              "TlRMTVNTUAABAAAAB4IIAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n"),

    // After calling trans->RestartWithAuth(), we should send a Type 3 message
    // (the credentials for the origin server).  The second request continues
    // on the same connection.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM TlRMTVNTUAADAAAAGAAYAGgAAAAYABgAgA"
              "AAAAAAAABAAAAAGAAYAEAAAAAQABAAWAAAAAAAAAAAAAAABYIIAHQA"
              "ZQBzAHQAaQBuAGcALQBuAHQAbABtAFcAVABDAC0AVwBJAE4ANwBO54"
              "dFMVvTHwAAAAAAAAAAAAAAAAAAAACS7sT6Uzw7L0L//WUqlIaVWpbI"
              "+4MUm7c=\r\n\r\n"),
  };

  MockRead data_reads3[] = {
    // The origin server responds with a Type 2 message.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM "
             "TlRMTVNTUAACAAAADAAMADgAAAAFgokCL24VN8dgOR8AAAAAAAAAALo"
             "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
             "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
             "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
             "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
             "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
             "BtAAAAAAA=\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Lastly we get the desired content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=utf-8\r\n"),
    MockRead("Content-Length: 13\r\n\r\n"),
    MockRead("Please Login\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  StaticSocketDataProvider data3(data_reads3, arraysize(data_reads3),
                                 data_writes3, arraysize(data_writes3));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckNTLMServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  // Enter the wrong password.
  rv = trans->RestartWithAuth(AuthCredentials(kTestingNTLM, kWrongPassword),
                              callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback3;
  rv = trans->RestartWithAuth(AuthCredentials(), callback3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  response = trans->GetResponseInfo();
  ASSERT_FALSE(response == NULL);
  EXPECT_TRUE(CheckNTLMServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback4;

  // Now enter the right password.
  rv = trans->RestartWithAuth(AuthCredentials(kTestingNTLM, kTestingNTLM),
                              callback4.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback4.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());

  TestCompletionCallback callback5;

  // One more roundtrip
  rv = trans->RestartWithAuth(AuthCredentials(), callback5.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback5.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(13, response->headers->GetContentLength());
}
#endif  // NTLM_PORTABLE

// Test reading a server response which has only headers, and no body.
// After some maximum number of bytes is consumed, the transaction should
// fail with ERR_RESPONSE_HEADERS_TOO_BIG.
TEST_P(HttpNetworkTransactionTest, LargeHeadersNoBody) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  // Respond with 300 kb of headers (we should fail after 256 kb).
  std::string large_headers_string;
  FillLargeHeadersString(&large_headers_string, 300 * 1024);

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead(ASYNC, large_headers_string.data(), large_headers_string.size()),
    MockRead("\r\nBODY"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_RESPONSE_HEADERS_TOO_BIG, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

// Make sure that we don't try to reuse a TCPClientSocket when failing to
// establish tunnel.
// http://code.google.com/p/chromium/issues/detail?id=3772
TEST_P(HttpNetworkTransactionTest,
       DontRecycleTransportSocketForSSLTunnel) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Configure against proxy server "myproxy:70".
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 404, using a persistent
  // connection. Usually a proxy would return 501 (not implemented),
  // or 200 (tunnel established).
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 404 Not Found\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  base::MessageLoop::current()->RunUntilIdle();

  // We now check to make sure the TCPClientSocket was not added back to
  // the pool.
  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));
  trans.reset();
  base::MessageLoop::current()->RunUntilIdle();
  // Make sure that the socket didn't get recycled after calling the destructor.
  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));
}

// Make sure that we recycle a socket after reading all of the response body.
TEST_P(HttpNetworkTransactionTest, RecycleSocket) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  MockRead data_reads[] = {
    // A part of the response body is received with the response headers.
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nhel"),
    // The rest of the response body is received in two parts.
    MockRead("lo"),
    MockRead(" world"),
    MockRead("junk"),  // Should not be read!!
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  std::string status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 200 OK", status_line);

  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  base::MessageLoop::current()->RunUntilIdle();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, GetIdleSocketCountInTransportSocketPool(session.get()));
}

// Make sure that we recycle a SSL socket after reading all of the response
// body.
TEST_P(HttpNetworkTransactionTest, RecycleSSLSocket) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 11\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  base::MessageLoop::current()->RunUntilIdle();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, GetIdleSocketCountInSSLSocketPool(session.get()));
}

// Grab a SSL socket, use it, and put it back into the pool.  Then, reuse it
// from the pool and make sure that we recover okay.
TEST_P(HttpNetworkTransactionTest, RecycleDeadSSLSocket) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 11\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead("hello world"),
    MockRead(ASYNC, 0, 0)   // EOF
  };

  SSLSocketDataProvider ssl(ASYNC, OK);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl2);

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  StaticSocketDataProvider data2(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  base::MessageLoop::current()->RunUntilIdle();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, GetIdleSocketCountInSSLSocketPool(session.get()));

  // Now start the second transaction, which should reuse the previous socket.

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));

  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  base::MessageLoop::current()->RunUntilIdle();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, GetIdleSocketCountInSSLSocketPool(session.get()));
}

// Make sure that we recycle a socket after a zero-length response.
// http://crbug.com/9880
TEST_P(HttpNetworkTransactionTest, RecycleSocketAfterZeroContentLength) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/csi?v=3&s=web&action=&"
                     "tran=undefined&ei=mAXcSeegAo-SMurloeUN&"
                     "e=17259,18167,19592,19773,19981,20133,20173,20233&"
                     "rt=prt.2642,ol.2649,xjs.2951");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 204 No Content\r\n"
             "Content-Length: 0\r\n"
             "Content-Type: text/html\r\n\r\n"),
    MockRead("junk"),  // Should not be read!!
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  std::string status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 204 No Content", status_line);

  EXPECT_EQ(0, GetIdleSocketCountInTransportSocketPool(session.get()));

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  base::MessageLoop::current()->RunUntilIdle();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, GetIdleSocketCountInTransportSocketPool(session.get()));
}

TEST_P(HttpNetworkTransactionTest, ResendRequestOnWriteBodyError) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request[2];
  // Transaction 1: a GET request that succeeds.  The socket is recycled
  // after use.
  request[0].method = "GET";
  request[0].url = GURL("http://www.google.com/");
  request[0].load_flags = 0;
  // Transaction 2: a POST request.  Reuses the socket kept alive from
  // transaction 1.  The first attempts fails when writing the POST data.
  // This causes the transaction to retry with a new socket.  The second
  // attempt succeeds.
  request[1].method = "POST";
  request[1].url = GURL("http://www.google.com/login.cgi");
  request[1].upload_data_stream = &upload_data_stream;
  request[1].load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // The first socket is used for transaction 1 and the first attempt of
  // transaction 2.

  // The response of transaction 1.
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  // The mock write results of transaction 1 and the first attempt of
  // transaction 2.
  MockWrite data_writes1[] = {
    MockWrite(SYNCHRONOUS, 64),  // GET
    MockWrite(SYNCHRONOUS, 93),  // POST
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_ABORTED),  // POST data
  };
  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));

  // The second socket is used for the second attempt of transaction 2.

  // The response of transaction 2.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\n"),
    MockRead("welcome"),
    MockRead(SYNCHRONOUS, OK),
  };
  // The mock write results of the second attempt of transaction 2.
  MockWrite data_writes2[] = {
    MockWrite(SYNCHRONOUS, 93),  // POST
    MockWrite(SYNCHRONOUS, 3),  // POST data
  };
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));

  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  const char* kExpectedResponseData[] = {
    "hello world", "welcome"
  };

  for (int i = 0; i < 2; ++i) {
    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    TestCompletionCallback callback;

    int rv = trans->Start(&request[i], callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

// Test the request-challenge-retry sequence for basic auth when there is
// an identity in the URL. The request should be sent as normal, but when
// it fails the identity from the URL is used to answer the challenge.
TEST_P(HttpNetworkTransactionTest, AuthIdentityInURL) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://foo:b@r@www.google.com/");
  request.load_flags = LOAD_NORMAL;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  // The password contains an escaped character -- for this test to pass it
  // will need to be unescaped by HttpNetworkTransaction.
  EXPECT_EQ("b%40r", request.url.password());

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  // After the challenge above, the transaction will be restarted using the
  // identity from the url (foo, b@r) to answer the challenge.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJAcg==\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  TestCompletionCallback callback1;
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(trans->IsReadyToRestartForAuth());

  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(AuthCredentials(), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  // There is no challenge info, since the identity in URL worked.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(100, response->headers->GetContentLength());

  // Empty the current queue.
  base::MessageLoop::current()->RunUntilIdle();
}

// Test the request-challenge-retry sequence for basic auth when there is an
// incorrect identity in the URL. The identity from the URL should be used only
// once.
TEST_P(HttpNetworkTransactionTest, WrongAuthIdentityInURL) {
  HttpRequestInfo request;
  request.method = "GET";
  // Note: the URL has a username:password in it.  The password "baz" is
  // wrong (should be "bar").
  request.url = GURL("http://foo:baz@www.google.com/");

  request.load_flags = LOAD_NORMAL;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  // After the challenge above, the transaction will be restarted using the
  // identity from the url (foo, baz) to answer the challenge.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  // After the challenge above, the transaction will be restarted using the
  // identity supplied by the user (foo, bar) to answer the challenge.
  MockWrite data_writes3[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads3[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  StaticSocketDataProvider data3(data_reads3, arraysize(data_reads3),
                                 data_writes3, arraysize(data_writes3));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(AuthCredentials(), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback3;
  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  // There is no challenge info, since the identity worked.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(100, response->headers->GetContentLength());

  // Empty the current queue.
  base::MessageLoop::current()->RunUntilIdle();
}


// Test the request-challenge-retry sequence for basic auth when there is a
// correct identity in the URL, but its use is being suppressed. The identity
// from the URL should never be used.
TEST_P(HttpNetworkTransactionTest, AuthIdentityInURLSuppressed) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://foo:bar@www.google.com/");
  request.load_flags = LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED),
  };

  // After the challenge above, the transaction will be restarted using the
  // identity supplied by the user, not the one in the URL, to answer the
  // challenge.
  MockWrite data_writes3[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads3[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data3(data_reads3, arraysize(data_reads3),
                                 data_writes3, arraysize(data_writes3));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);

  TestCompletionCallback callback1;
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback3;
  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  // There is no challenge info, since the identity worked.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());

  // Empty the current queue.
  base::MessageLoop::current()->RunUntilIdle();
}

// Test that previously tried username/passwords for a realm get re-used.
TEST_P(HttpNetworkTransactionTest, BasicAuthCacheAndPreauth) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Transaction 1: authenticate (foo, bar) on MyRealm1
  {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/y/z");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/z HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(SYNCHRONOUS, ERR_FAILED),
    };

    // Resend with authorization (username=foo, password=bar)
    MockWrite data_writes2[] = {
      MockWrite("GET /x/y/z HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);
    session_deps_.socket_factory->AddSocketDataProvider(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(
        AuthCredentials(kFoo, kBar), callback2.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 2: authenticate (foo2, bar2) on MyRealm2
  {
    HttpRequestInfo request;
    request.method = "GET";
    // Note that Transaction 1 was at /x/y/z, so this is in the same
    // protection space as MyRealm1.
    request.url = GURL("http://www.google.com/x/y/a/b");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/a/b HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                // Send preemptive authorization for MyRealm1
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // The server didn't like the preemptive authorization, and
    // challenges us for a different realm (MyRealm2).
    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm2\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(SYNCHRONOUS, ERR_FAILED),
    };

    // Resend with authorization for MyRealm2 (username=foo2, password=bar2)
    MockWrite data_writes2[] = {
      MockWrite("GET /x/y/a/b HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vMjpiYXIy\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);
    session_deps_.socket_factory->AddSocketDataProvider(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    ASSERT_TRUE(response->auth_challenge.get());
    EXPECT_FALSE(response->auth_challenge->is_proxy);
    EXPECT_EQ("www.google.com:80",
              response->auth_challenge->challenger.ToString());
    EXPECT_EQ("MyRealm2", response->auth_challenge->realm);
    EXPECT_EQ("basic", response->auth_challenge->scheme);

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(
        AuthCredentials(kFoo2, kBar2), callback2.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 3: Resend a request in MyRealm's protection space --
  // succeed with preemptive authorization.
  {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/y/z2");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/z2 HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                // The authorization for MyRealm1 gets sent preemptively
                // (since the url is in the same protection space)
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever accepts the preemptive authorization
    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);

    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 4: request another URL in MyRealm (however the
  // url is not known to belong to the protection space, so no pre-auth).
  {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/1");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /x/1 HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(SYNCHRONOUS, ERR_FAILED),
    };

    // Resend with authorization from MyRealm's cache.
    MockWrite data_writes2[] = {
      MockWrite("GET /x/1 HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);
    session_deps_.socket_factory->AddSocketDataProvider(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    EXPECT_TRUE(trans->IsReadyToRestartForAuth());
    TestCompletionCallback callback2;
    rv = trans->RestartWithAuth(AuthCredentials(), callback2.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_FALSE(trans->IsReadyToRestartForAuth());

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 5: request a URL in MyRealm, but the server rejects the
  // cached identity. Should invalidate and re-prompt.
  {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/p/q/t");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /p/q/t HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(SYNCHRONOUS, ERR_FAILED),
    };

    // Resend with authorization from cache for MyRealm.
    MockWrite data_writes2[] = {
      MockWrite("GET /p/q/t HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever rejects the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(SYNCHRONOUS, ERR_FAILED),
    };

    // At this point we should prompt for new credentials for MyRealm.
    // Restart with username=foo3, password=foo4.
    MockWrite data_writes3[] = {
      MockWrite("GET /p/q/t HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vMzpiYXIz\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads3[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    StaticSocketDataProvider data3(data_reads3, arraysize(data_reads3),
                                   data_writes3, arraysize(data_writes3));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);
    session_deps_.socket_factory->AddSocketDataProvider(&data2);
    session_deps_.socket_factory->AddSocketDataProvider(&data3);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    EXPECT_TRUE(trans->IsReadyToRestartForAuth());
    TestCompletionCallback callback2;
    rv = trans->RestartWithAuth(AuthCredentials(), callback2.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_FALSE(trans->IsReadyToRestartForAuth());

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

    TestCompletionCallback callback3;

    rv = trans->RestartWithAuth(
        AuthCredentials(kFoo3, kBar3), callback3.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback3.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }
}

// Tests that nonce count increments when multiple auth attempts
// are started with the same nonce.
TEST_P(HttpNetworkTransactionTest, DigestPreAuthNonceCount) {
  HttpAuthHandlerDigest::Factory* digest_factory =
      new HttpAuthHandlerDigest::Factory();
  HttpAuthHandlerDigest::FixedNonceGenerator* nonce_generator =
      new HttpAuthHandlerDigest::FixedNonceGenerator("0123456789abcdef");
  digest_factory->set_nonce_generator(nonce_generator);
  session_deps_.http_auth_handler_factory.reset(digest_factory);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Transaction 1: authenticate (foo, bar) on MyRealm1
  {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/y/z");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/z HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Digest realm=\"digestive\", nonce=\"OU812\", "
               "algorithm=MD5, qop=\"auth\"\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    // Resend with authorization (username=foo, password=bar)
    MockWrite data_writes2[] = {
      MockWrite("GET /x/y/z HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Digest username=\"foo\", realm=\"digestive\", "
                "nonce=\"OU812\", uri=\"/x/y/z\", algorithm=MD5, "
                "response=\"03ffbcd30add722589c1de345d7a927f\", qop=auth, "
                "nc=00000001, cnonce=\"0123456789abcdef\"\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                   data_writes2, arraysize(data_writes2));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);
    session_deps_.socket_factory->AddSocketDataProvider(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(CheckDigestServerAuth(response->auth_challenge.get()));

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(
        AuthCredentials(kFoo, kBar), callback2.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
  }

  // ------------------------------------------------------------------------

  // Transaction 2: Request another resource in digestive's protection space.
  // This will preemptively add an Authorization header which should have an
  // "nc" value of 2 (as compared to 1 in the first use.
  {
    HttpRequestInfo request;
    request.method = "GET";
    // Note that Transaction 1 was at /x/y/z, so this is in the same
    // protection space as digest.
    request.url = GURL("http://www.google.com/x/y/a/b");
    request.load_flags = 0;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/a/b HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Digest username=\"foo\", realm=\"digestive\", "
                "nonce=\"OU812\", uri=\"/x/y/a/b\", algorithm=MD5, "
                "response=\"d6f9a2c07d1c5df7b89379dca1269b35\", qop=auth, "
                "nc=00000002, cnonce=\"0123456789abcdef\"\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
    };

    StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                   data_writes1, arraysize(data_writes1));
    session_deps_.socket_factory->AddSocketDataProvider(&data1);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
  }
}

// Test the ResetStateForRestart() private method.
TEST_P(HttpNetworkTransactionTest, ResetStateForRestart) {
  // Create a transaction (the dependencies aren't important).
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  // Setup some state (which we expect ResetStateForRestart() will clear).
  trans->read_buf_ = new IOBuffer(15);
  trans->read_buf_len_ = 15;
  trans->request_headers_.SetHeader("Authorization", "NTLM");

  // Setup state in response_
  HttpResponseInfo* response = &trans->response_;
  response->auth_challenge = new AuthChallengeInfo();
  response->ssl_info.cert_status = static_cast<CertStatus>(-1);  // Nonsensical.
  response->response_time = base::Time::Now();
  response->was_cached = true;  // (Wouldn't ever actually be true...)

  { // Setup state for response_.vary_data
    HttpRequestInfo request;
    std::string temp("HTTP/1.1 200 OK\nVary: foo, bar\n\n");
    std::replace(temp.begin(), temp.end(), '\n', '\0');
    scoped_refptr<HttpResponseHeaders> headers(new HttpResponseHeaders(temp));
    request.extra_headers.SetHeader("Foo", "1");
    request.extra_headers.SetHeader("bar", "23");
    EXPECT_TRUE(response->vary_data.Init(request, *headers.get()));
  }

  // Cause the above state to be reset.
  trans->ResetStateForRestart();

  // Verify that the state that needed to be reset, has been reset.
  EXPECT_TRUE(trans->read_buf_.get() == NULL);
  EXPECT_EQ(0, trans->read_buf_len_);
  EXPECT_TRUE(trans->request_headers_.IsEmpty());
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_TRUE(response->headers.get() == NULL);
  EXPECT_FALSE(response->was_cached);
  EXPECT_EQ(0U, response->ssl_info.cert_status);
  EXPECT_FALSE(response->vary_data.is_valid());
}

// Test HTTPS connections to a site with a bad certificate
TEST_P(HttpNetworkTransactionTest, HTTPSBadCertificate) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider ssl_bad_certificate;
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  SSLSocketDataProvider ssl_bad(ASYNC, ERR_CERT_AUTHORITY_INVALID);
  SSLSocketDataProvider ssl(ASYNC, OK);

  session_deps_.socket_factory->AddSocketDataProvider(&ssl_bad_certificate);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_bad);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, rv);

  rv = trans->RestartIgnoringLastError(callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test HTTPS connections to a site with a bad certificate, going through a
// proxy
TEST_P(HttpNetworkTransactionTest, HTTPSBadCertificateViaProxy) {
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite proxy_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead proxy_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK)
  };

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider ssl_bad_certificate(
      proxy_reads, arraysize(proxy_reads),
      proxy_writes, arraysize(proxy_writes));
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  SSLSocketDataProvider ssl_bad(ASYNC, ERR_CERT_AUTHORITY_INVALID);
  SSLSocketDataProvider ssl(ASYNC, OK);

  session_deps_.socket_factory->AddSocketDataProvider(&ssl_bad_certificate);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_bad);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  for (int i = 0; i < 2; i++) {
    session_deps_.socket_factory->ResetNextMockIndexes();

    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

    int rv = trans->Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, rv);

    rv = trans->RestartIgnoringLastError(callback.callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();

    ASSERT_TRUE(response != NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }
}


// Test HTTPS connections to a site, going through an HTTPS proxy
TEST_P(HttpNetworkTransactionTest, HTTPSViaHttpsProxy) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("HTTPS proxy:70"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  SSLSocketDataProvider proxy_ssl(ASYNC, OK);  // SSL to the proxy
  SSLSocketDataProvider tunnel_ssl(ASYNC, OK);  // SSL through the tunnel

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy_ssl);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&tunnel_ssl);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(100, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);
}

// Test an HTTPS Proxy's ability to redirect a CONNECT request
TEST_P(HttpNetworkTransactionTest, RedirectOfHttpsConnectViaHttpsProxy) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("HTTPS proxy:70"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 302 Redirect\r\n"),
    MockRead("Location: http://login.example.com/\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  SSLSocketDataProvider proxy_ssl(ASYNC, OK);  // SSL to the proxy

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy_ssl);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);

  EXPECT_EQ(302, response->headers->response_code());
  std::string url;
  EXPECT_TRUE(response->headers->IsRedirect(&url));
  EXPECT_EQ("http://login.example.com/", url);

  // In the case of redirects from proxies, HttpNetworkTransaction returns
  // timing for the proxy connection instead of the connection to the host,
  // and no send / receive times.
  // See HttpNetworkTransaction::OnHttpsProxyTunnelResponse.
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.connect_timing.connect_start);
  ExpectConnectTimingHasTimes(
      load_timing_info.connect_timing,
      CONNECT_TIMING_HAS_DNS_TIMES | CONNECT_TIMING_HAS_SSL_TIMES);

  EXPECT_TRUE(load_timing_info.send_start.is_null());
  EXPECT_TRUE(load_timing_info.send_end.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Test an HTTPS (SPDY) Proxy's ability to redirect a CONNECT request
TEST_P(HttpNetworkTransactionTest, RedirectOfHttpsConnectViaSpdyProxy) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://proxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  scoped_ptr<SpdyFrame> conn(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                             LOWEST));
  scoped_ptr<SpdyFrame> goaway(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite data_writes[] = {
    CreateMockWrite(*conn.get(), 0, SYNCHRONOUS),
    CreateMockWrite(*goaway.get(), 3, SYNCHRONOUS),
  };

  static const char* const kExtraHeaders[] = {
    "location",
    "http://login.example.com/",
  };
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdySynReplyError("302 Redirect", kExtraHeaders,
                                 arraysize(kExtraHeaders)/2, 1));
  MockRead data_reads[] = {
    CreateMockRead(*resp.get(), 1, SYNCHRONOUS),
    MockRead(ASYNC, 0, 2),  // EOF
  };

  DelayedSocketData data(
      1,  // wait for one write to finish before reading.
      data_reads, arraysize(data_reads),
      data_writes, arraysize(data_writes));
  SSLSocketDataProvider proxy_ssl(ASYNC, OK);  // SSL to the proxy
  proxy_ssl.SetNextProto(GetParam());

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy_ssl);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);

  EXPECT_EQ(302, response->headers->response_code());
  std::string url;
  EXPECT_TRUE(response->headers->IsRedirect(&url));
  EXPECT_EQ("http://login.example.com/", url);
}

// Test that an HTTPS proxy's response to a CONNECT request is filtered.
TEST_P(HttpNetworkTransactionTest,
       ErrorResponseToHttpsConnectViaHttpsProxy) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://proxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 404 Not Found\r\n"),
    MockRead("Content-Length: 23\r\n\r\n"),
    MockRead("The host does not exist"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  SSLSocketDataProvider proxy_ssl(ASYNC, OK);  // SSL to the proxy

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy_ssl);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);

  // TODO(ttuttle): Anything else to check here?
}

// Test that a SPDY proxy's response to a CONNECT request is filtered.
TEST_P(HttpNetworkTransactionTest,
       ErrorResponseToHttpsConnectViaSpdyProxy) {
  session_deps_.proxy_service.reset(
     ProxyService::CreateFixed("https://proxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  scoped_ptr<SpdyFrame> conn(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                             LOWEST));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite data_writes[] = {
    CreateMockWrite(*conn.get(), 0, SYNCHRONOUS),
    CreateMockWrite(*rst.get(), 3, SYNCHRONOUS),
  };

  static const char* const kExtraHeaders[] = {
    "location",
    "http://login.example.com/",
  };
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdySynReplyError("404 Not Found", kExtraHeaders,
                                 arraysize(kExtraHeaders)/2, 1));
  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(
          1, "The host does not exist", 23, true));
  MockRead data_reads[] = {
    CreateMockRead(*resp.get(), 1, SYNCHRONOUS),
    CreateMockRead(*body.get(), 2, SYNCHRONOUS),
    MockRead(ASYNC, 0, 4),  // EOF
  };

  DelayedSocketData data(
      1,  // wait for one write to finish before reading.
      data_reads, arraysize(data_reads),
      data_writes, arraysize(data_writes));
  SSLSocketDataProvider proxy_ssl(ASYNC, OK);  // SSL to the proxy
  proxy_ssl.SetNextProto(GetParam());

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy_ssl);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);

  // TODO(ttuttle): Anything else to check here?
}

// Test the request-challenge-retry sequence for basic auth, through
// a SPDY proxy over a single SPDY session.
TEST_P(HttpNetworkTransactionTest, BasicAuthSpdyProxy) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  // when the no authentication data flag is set.
  request.load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // Configure against https proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("HTTPS myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Since we have proxy, should try to establish tunnel.
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                            LOWEST));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));

  // After calling trans->RestartWithAuth(), this is the request we should
  // be issuing -- the final header line contains the credentials.
  const char* const kAuthCredentials[] = {
      "proxy-authorization", "Basic Zm9vOmJhcg==",
  };
  scoped_ptr<SpdyFrame> connect2(spdy_util_.ConstructSpdyConnect(
      kAuthCredentials, arraysize(kAuthCredentials) / 2, 3, LOWEST));
  // fetch https://www.google.com/ via HTTP
  const char get[] = "GET / HTTP/1.1\r\n"
    "Host: www.google.com\r\n"
    "Connection: keep-alive\r\n\r\n";
  scoped_ptr<SpdyFrame> wrapped_get(
      spdy_util_.ConstructSpdyBodyFrame(3, get, strlen(get), false));

  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 1, ASYNC),
    CreateMockWrite(*rst, 4, ASYNC),
    CreateMockWrite(*connect2, 5),
    CreateMockWrite(*wrapped_get, 8),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  const char* const kAuthChallenge[] = {
    spdy_util_.GetStatusKey(), "407 Proxy Authentication Required",
    spdy_util_.GetVersionKey(), "HTTP/1.1",
    "proxy-authenticate", "Basic realm=\"MyRealm1\"",
  };

  scoped_ptr<SpdyFrame> conn_auth_resp(
      spdy_util_.ConstructSpdyControlFrame(NULL,
                                           0,
                                           false,
                                           1,
                                           LOWEST,
                                           SYN_REPLY,
                                           CONTROL_FLAG_NONE,
                                           kAuthChallenge,
                                           arraysize(kAuthChallenge),
                                           0));

  scoped_ptr<SpdyFrame> conn_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  const char resp[] = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 5\r\n\r\n";

  scoped_ptr<SpdyFrame> wrapped_get_resp(
      spdy_util_.ConstructSpdyBodyFrame(3, resp, strlen(resp), false));
  scoped_ptr<SpdyFrame> wrapped_body(
      spdy_util_.ConstructSpdyBodyFrame(3, "hello", 5, false));
  MockRead spdy_reads[] = {
    CreateMockRead(*conn_auth_resp, 2, ASYNC),
    CreateMockRead(*conn_resp, 6, ASYNC),
    CreateMockRead(*wrapped_get_resp, 9, ASYNC),
    CreateMockRead(*wrapped_body, 10, ASYNC),
    MockRead(ASYNC, OK, 11),  // EOF.  May or may not be read.
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);
  // Negotiate SPDY to the proxy
  SSLSocketDataProvider proxy(ASYNC, OK);
  proxy.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy);
  // Vanilla SSL to the server
  SSLSocketDataProvider server(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&server);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLog::PHASE_NONE);
  ExpectLogContainsSomewhere(
      entries, pos,
      NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLog::PHASE_NONE);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_FALSE(response->headers.get() == NULL);
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());
  EXPECT_TRUE(response->auth_challenge.get() != NULL);
  EXPECT_TRUE(CheckBasicProxyAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(AuthCredentials(kFoo, kBar),
                              callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(5, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should not be set.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);

  trans.reset();
  session->CloseAllConnections();
}

// Test that an explicitly trusted SPDY proxy can push a resource from an
// origin that is different from that of its associated resource.
TEST_P(HttpNetworkTransactionTest, CrossOriginProxyPush) {
  HttpRequestInfo request;
  HttpRequestInfo push_request;

  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  push_request.method = "GET";
  push_request.url = GURL("http://www.another-origin.com/foo.dat");

  // Configure against https proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("HTTPS myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();

  // Enable cross-origin push.
  session_deps_.trusted_spdy_proxy = "myproxy:70";

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, false));

  MockWrite spdy_writes[] = {
    CreateMockWrite(*stream1_syn, 1, ASYNC),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  scoped_ptr<SpdyFrame>
      stream1_body(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.another-origin.com/foo.dat"));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));

  MockRead spdy_reads[] = {
    CreateMockRead(*stream1_reply, 2, ASYNC),
    CreateMockRead(*stream2_syn, 3, ASYNC),
    CreateMockRead(*stream1_body, 4, ASYNC),
    CreateMockRead(*stream2_body, 5, ASYNC),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);
  // Negotiate SPDY to the proxy
  SSLSocketDataProvider proxy(ASYNC, OK);
  proxy.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy);

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request, callback.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();

  scoped_ptr<HttpTransaction> push_trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  rv = push_trans->Start(&push_request, callback.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* push_response = push_trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->headers->IsKeepAlive());

  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello!", response_data);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  // Verify the pushed stream.
  EXPECT_TRUE(push_response->headers.get() != NULL);
  EXPECT_EQ(200, push_response->headers->response_code());

  rv = ReadTransaction(push_trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("pushed", response_data);

  LoadTimingInfo push_load_timing_info;
  EXPECT_TRUE(push_trans->GetLoadTimingInfo(&push_load_timing_info));
  TestLoadTimingReusedWithPac(push_load_timing_info);
  // The transactions should share a socket ID, despite being for different
  // origins.
  EXPECT_EQ(load_timing_info.socket_log_id,
            push_load_timing_info.socket_log_id);

  trans.reset();
  push_trans.reset();
  session->CloseAllConnections();
}

// Test that an explicitly trusted SPDY proxy cannot push HTTPS content.
TEST_P(HttpNetworkTransactionTest, CrossOriginProxyPushCorrectness) {
  HttpRequestInfo request;

  request.method = "GET";
  request.url = GURL("http://www.google.com/");

  // Configure against https proxy server "myproxy:70".
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();

  // Enable cross-origin push.
  session_deps_.trusted_spdy_proxy = "myproxy:70";

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, false));

  scoped_ptr<SpdyFrame> push_rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_REFUSED_STREAM));

  MockWrite spdy_writes[] = {
    CreateMockWrite(*stream1_syn, 1, ASYNC),
    CreateMockWrite(*push_rst, 4),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  scoped_ptr<SpdyFrame>
      stream1_body(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "https://www.another-origin.com/foo.dat"));

  MockRead spdy_reads[] = {
    CreateMockRead(*stream1_reply, 2, ASYNC),
    CreateMockRead(*stream2_syn, 3, ASYNC),
    CreateMockRead(*stream1_body, 5, ASYNC),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);
  // Negotiate SPDY to the proxy
  SSLSocketDataProvider proxy(ASYNC, OK);
  proxy.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&proxy);

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request, callback.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->headers->IsKeepAlive());

  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello!", response_data);

  trans.reset();
  session->CloseAllConnections();
}

// Test HTTPS connections to a site with a bad certificate, going through an
// HTTPS proxy
TEST_P(HttpNetworkTransactionTest, HTTPSBadCertificateViaHttpsProxy) {
  session_deps_.proxy_service.reset(ProxyService::CreateFixed(
      "https://proxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Attempt to fetch the URL from a server with a bad cert
  MockWrite bad_cert_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead bad_cert_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK)
  };

  // Attempt to fetch the URL with a good cert
  MockWrite good_data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead good_cert_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider ssl_bad_certificate(
      bad_cert_reads, arraysize(bad_cert_reads),
      bad_cert_writes, arraysize(bad_cert_writes));
  StaticSocketDataProvider data(good_cert_reads, arraysize(good_cert_reads),
                                good_data_writes, arraysize(good_data_writes));
  SSLSocketDataProvider ssl_bad(ASYNC, ERR_CERT_AUTHORITY_INVALID);
  SSLSocketDataProvider ssl(ASYNC, OK);

  // SSL to the proxy, then CONNECT request, then SSL with bad certificate
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&ssl_bad_certificate);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_bad);

  // SSL to the proxy, then CONNECT request, then valid SSL certificate
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, rv);

  rv = trans->RestartIgnoringLastError(callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();

  ASSERT_TRUE(response != NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_UserAgent) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                  "Chromium Ultra Awesome X Edition");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Chromium Ultra Awesome X Edition\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_UserAgentOverTunnel) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                  "Chromium Ultra Awesome X Edition");

  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "User-Agent: Chromium Ultra Awesome X Edition\r\n\r\n"),
  };
  MockRead data_reads[] = {
    // Return an error, so the transaction stops here (this test isn't
    // interested in the rest).
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Proxy-Connection: close\r\n\r\n"),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_Referer) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  request.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                  "http://the.previous.site.com/");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Referer: http://the.previous.site.com/\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_PostContentLengthZero) {
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_PutContentLengthZero) {
  HttpRequestInfo request;
  request.method = "PUT";
  request.url = GURL("http://www.google.com/");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("PUT / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_HeadContentLengthZero) {
  HttpRequestInfo request;
  request.method = "HEAD";
  request.url = GURL("http://www.google.com/");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("HEAD / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_CacheControlNoCache) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = LOAD_BYPASS_CACHE;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Pragma: no-cache\r\n"
              "Cache-Control: no-cache\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest,
       BuildRequest_CacheControlValidateCache) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = LOAD_VALIDATE_CACHE;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Cache-Control: max-age=0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_ExtraHeaders) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.extra_headers.SetHeader("FooHeader", "Bar");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "FooHeader: Bar\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, BuildRequest_ExtraHeadersStripped) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.extra_headers.SetHeader("referer", "www.foo.com");
  request.extra_headers.SetHeader("hEllo", "Kitty");
  request.extra_headers.SetHeader("FoO", "bar");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "referer: www.foo.com\r\n"
              "hEllo: Kitty\r\n"
              "FoO: bar\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_P(HttpNetworkTransactionTest, SOCKS4_HTTP_GET) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("SOCKS myproxy:1080"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  char write_buffer[] = { 0x04, 0x01, 0x00, 0x50, 127, 0, 0, 1, 0 };
  char read_buffer[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
    MockWrite(ASYNC, write_buffer, arraysize(write_buffer)),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockRead(ASYNC, read_buffer, arraysize(read_buffer)),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(SYNCHRONOUS, OK)
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

TEST_P(HttpNetworkTransactionTest, SOCKS4_SSL_GET) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("SOCKS myproxy:1080"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  unsigned char write_buffer[] = { 0x04, 0x01, 0x01, 0xBB, 127, 0, 0, 1, 0 };
  unsigned char read_buffer[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
    MockWrite(ASYNC, reinterpret_cast<char*>(write_buffer),
              arraysize(write_buffer)),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockRead(ASYNC, reinterpret_cast<char*>(read_buffer),
             arraysize(read_buffer)),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(SYNCHRONOUS, OK)
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

TEST_P(HttpNetworkTransactionTest, SOCKS4_HTTP_GET_no_PAC) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("socks4://myproxy:1080"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  char write_buffer[] = { 0x04, 0x01, 0x00, 0x50, 127, 0, 0, 1, 0 };
  char read_buffer[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
    MockWrite(ASYNC, write_buffer, arraysize(write_buffer)),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockRead(ASYNC, read_buffer, arraysize(read_buffer)),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(SYNCHRONOUS, OK)
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReused(load_timing_info,
                          CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

TEST_P(HttpNetworkTransactionTest, SOCKS5_HTTP_GET) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("SOCKS5 myproxy:1080"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
  const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };
  const char kSOCKS5OkRequest[] = {
    0x05,  // Version
    0x01,  // Command (CONNECT)
    0x00,  // Reserved.
    0x03,  // Address type (DOMAINNAME).
    0x0E,  // Length of domain (14)
    // Domain string:
    'w', 'w', 'w', '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o', 'm',
    0x00, 0x50,  // 16-bit port (80)
  };
  const char kSOCKS5OkResponse[] =
      { 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };

  MockWrite data_writes[] = {
    MockWrite(ASYNC, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
    MockWrite(ASYNC, kSOCKS5OkRequest, arraysize(kSOCKS5OkRequest)),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockRead(ASYNC, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
    MockRead(ASYNC, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(SYNCHRONOUS, OK)
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

TEST_P(HttpNetworkTransactionTest, SOCKS5_SSL_GET) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("SOCKS5 myproxy:1080"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
  const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };
  const unsigned char kSOCKS5OkRequest[] = {
    0x05,  // Version
    0x01,  // Command (CONNECT)
    0x00,  // Reserved.
    0x03,  // Address type (DOMAINNAME).
    0x0E,  // Length of domain (14)
    // Domain string:
    'w', 'w', 'w', '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o', 'm',
    0x01, 0xBB,  // 16-bit port (443)
  };

  const char kSOCKS5OkResponse[] =
      { 0x05, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0x00, 0x00 };

  MockWrite data_writes[] = {
    MockWrite(ASYNC, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
    MockWrite(ASYNC, reinterpret_cast<const char*>(kSOCKS5OkRequest),
              arraysize(kSOCKS5OkRequest)),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockRead(ASYNC, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
    MockRead(ASYNC, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(SYNCHRONOUS, OK)
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

namespace {

// Tests that for connection endpoints the group names are correctly set.

struct GroupNameTest {
  std::string proxy_server;
  std::string url;
  std::string expected_group_name;
  bool ssl;
};

scoped_refptr<HttpNetworkSession> SetupSessionForGroupNameTests(
    NextProto next_proto,
    SpdySessionDependencies* session_deps_) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  http_server_properties->SetAlternateProtocol(
      HostPortPair("host.with.alternate", 80), 443,
      AlternateProtocolFromNextProto(next_proto));

  return session;
}

int GroupNameTransactionHelper(
    const std::string& url,
    const scoped_refptr<HttpNetworkSession>& session) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL(url);
  request.load_flags = 0;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  TestCompletionCallback callback;

  // We do not complete this request, the dtor will clean the transaction up.
  return trans->Start(&request, callback.callback(), BoundNetLog());
}

}  // namespace

TEST_P(HttpNetworkTransactionTest, GroupNameForDirectConnections) {
  const GroupNameTest tests[] = {
    {
      "",  // unused
      "http://www.google.com/direct",
      "www.google.com:80",
      false,
    },
    {
      "",  // unused
      "http://[2001:1418:13:1::25]/direct",
      "[2001:1418:13:1::25]:80",
      false,
    },

    // SSL Tests
    {
      "",  // unused
      "https://www.google.com/direct_ssl",
      "ssl/www.google.com:443",
      true,
    },
    {
      "",  // unused
      "https://[2001:1418:13:1::25]/direct",
      "ssl/[2001:1418:13:1::25]:443",
      true,
    },
    {
      "",  // unused
      "http://host.with.alternate/direct",
      "ssl/host.with.alternate:443",
      true,
    },
  };

  session_deps_.use_alternate_protocols = true;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    session_deps_.proxy_service.reset(
        ProxyService::CreateFixed(tests[i].proxy_server));
    scoped_refptr<HttpNetworkSession> session(
        SetupSessionForGroupNameTests(GetParam(), &session_deps_));

    HttpNetworkSessionPeer peer(session);
    CaptureGroupNameTransportSocketPool* transport_conn_pool =
        new CaptureGroupNameTransportSocketPool(NULL, NULL);
    CaptureGroupNameSSLSocketPool* ssl_conn_pool =
        new CaptureGroupNameSSLSocketPool(NULL, NULL);
    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetTransportSocketPool(transport_conn_pool);
    mock_pool_manager->SetSSLSocketPool(ssl_conn_pool);
    peer.SetClientSocketPoolManager(
        mock_pool_manager.PassAs<ClientSocketPoolManager>());

    EXPECT_EQ(ERR_IO_PENDING,
              GroupNameTransactionHelper(tests[i].url, session));
    if (tests[i].ssl)
      EXPECT_EQ(tests[i].expected_group_name,
                ssl_conn_pool->last_group_name_received());
    else
      EXPECT_EQ(tests[i].expected_group_name,
                transport_conn_pool->last_group_name_received());
  }

}

TEST_P(HttpNetworkTransactionTest, GroupNameForHTTPProxyConnections) {
  const GroupNameTest tests[] = {
    {
      "http_proxy",
      "http://www.google.com/http_proxy_normal",
      "www.google.com:80",
      false,
    },

    // SSL Tests
    {
      "http_proxy",
      "https://www.google.com/http_connect_ssl",
      "ssl/www.google.com:443",
      true,
    },

    {
      "http_proxy",
      "http://host.with.alternate/direct",
      "ssl/host.with.alternate:443",
      true,
    },

    {
      "http_proxy",
      "ftp://ftp.google.com/http_proxy_normal",
      "ftp/ftp.google.com:21",
      false,
    },
  };

  session_deps_.use_alternate_protocols = true;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    session_deps_.proxy_service.reset(
        ProxyService::CreateFixed(tests[i].proxy_server));
    scoped_refptr<HttpNetworkSession> session(
        SetupSessionForGroupNameTests(GetParam(), &session_deps_));

    HttpNetworkSessionPeer peer(session);

    HostPortPair proxy_host("http_proxy", 80);
    CaptureGroupNameHttpProxySocketPool* http_proxy_pool =
        new CaptureGroupNameHttpProxySocketPool(NULL, NULL);
    CaptureGroupNameSSLSocketPool* ssl_conn_pool =
        new CaptureGroupNameSSLSocketPool(NULL, NULL);

    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetSocketPoolForHTTPProxy(proxy_host, http_proxy_pool);
    mock_pool_manager->SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    peer.SetClientSocketPoolManager(
        mock_pool_manager.PassAs<ClientSocketPoolManager>());

    EXPECT_EQ(ERR_IO_PENDING,
              GroupNameTransactionHelper(tests[i].url, session));
    if (tests[i].ssl)
      EXPECT_EQ(tests[i].expected_group_name,
                ssl_conn_pool->last_group_name_received());
    else
      EXPECT_EQ(tests[i].expected_group_name,
                http_proxy_pool->last_group_name_received());
  }
}

TEST_P(HttpNetworkTransactionTest, GroupNameForSOCKSConnections) {
  const GroupNameTest tests[] = {
    {
      "socks4://socks_proxy:1080",
      "http://www.google.com/socks4_direct",
      "socks4/www.google.com:80",
      false,
    },
    {
      "socks5://socks_proxy:1080",
      "http://www.google.com/socks5_direct",
      "socks5/www.google.com:80",
      false,
    },

    // SSL Tests
    {
      "socks4://socks_proxy:1080",
      "https://www.google.com/socks4_ssl",
      "socks4/ssl/www.google.com:443",
      true,
    },
    {
      "socks5://socks_proxy:1080",
      "https://www.google.com/socks5_ssl",
      "socks5/ssl/www.google.com:443",
      true,
    },

    {
      "socks4://socks_proxy:1080",
      "http://host.with.alternate/direct",
      "socks4/ssl/host.with.alternate:443",
      true,
    },
  };

  session_deps_.use_alternate_protocols = true;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    session_deps_.proxy_service.reset(
        ProxyService::CreateFixed(tests[i].proxy_server));
    scoped_refptr<HttpNetworkSession> session(
        SetupSessionForGroupNameTests(GetParam(), &session_deps_));

    HttpNetworkSessionPeer peer(session);

    HostPortPair proxy_host("socks_proxy", 1080);
    CaptureGroupNameSOCKSSocketPool* socks_conn_pool =
        new CaptureGroupNameSOCKSSocketPool(NULL, NULL);
    CaptureGroupNameSSLSocketPool* ssl_conn_pool =
        new CaptureGroupNameSSLSocketPool(NULL, NULL);

    scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
        new MockClientSocketPoolManager);
    mock_pool_manager->SetSocketPoolForSOCKSProxy(proxy_host, socks_conn_pool);
    mock_pool_manager->SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    peer.SetClientSocketPoolManager(
        mock_pool_manager.PassAs<ClientSocketPoolManager>());

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    EXPECT_EQ(ERR_IO_PENDING,
              GroupNameTransactionHelper(tests[i].url, session));
    if (tests[i].ssl)
      EXPECT_EQ(tests[i].expected_group_name,
                ssl_conn_pool->last_group_name_received());
    else
      EXPECT_EQ(tests[i].expected_group_name,
                socks_conn_pool->last_group_name_received());
  }
}

TEST_P(HttpNetworkTransactionTest, ReconsiderProxyAfterFailedConnection) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("myproxy:70;foobar:80"));

  // This simulates failure resolving all hostnames; that means we will fail
  // connecting to both proxies (myproxy:70 and foobar:80).
  session_deps_.host_resolver->rules()->AddSimulatedFailure("*");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, rv);
}

// Base test to make sure that when the load flags for a request specify to
// bypass the cache, the DNS cache is not used.
void HttpNetworkTransactionTest::BypassHostCacheOnRefreshHelper(
    int load_flags) {
  // Issue a request, asking to bypass the cache(s).
  HttpRequestInfo request;
  request.method = "GET";
  request.load_flags = load_flags;
  request.url = GURL("http://www.google.com/");

  // Select a host resolver that does caching.
  session_deps_.host_resolver.reset(new MockCachingHostResolver);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  // Warm up the host cache so it has an entry for "www.google.com".
  AddressList addrlist;
  TestCompletionCallback callback;
  int rv = session_deps_.host_resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &addrlist,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that it was added to host cache, by doing a subsequent async lookup
  // and confirming it completes synchronously.
  rv = session_deps_.host_resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &addrlist,
      callback.callback(),
      NULL,
      BoundNetLog());
  ASSERT_EQ(OK, rv);

  // Inject a failure the next time that "www.google.com" is resolved. This way
  // we can tell if the next lookup hit the cache, or the "network".
  // (cache --> success, "network" --> failure).
  session_deps_.host_resolver->rules()->AddSimulatedFailure("www.google.com");

  // Connect up a mock socket which will fail with ERR_UNEXPECTED during the
  // first read -- this won't be reached as the host resolution will fail first.
  MockRead data_reads[] = { MockRead(SYNCHRONOUS, ERR_UNEXPECTED) };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  // Run the request.
  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  // If we bypassed the cache, we would have gotten a failure while resolving
  // "www.google.com".
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
}

// There are multiple load flags that should trigger the host cache bypass.
// Test each in isolation:
TEST_P(HttpNetworkTransactionTest, BypassHostCacheOnRefresh1) {
  BypassHostCacheOnRefreshHelper(LOAD_BYPASS_CACHE);
}

TEST_P(HttpNetworkTransactionTest, BypassHostCacheOnRefresh2) {
  BypassHostCacheOnRefreshHelper(LOAD_VALIDATE_CACHE);
}

TEST_P(HttpNetworkTransactionTest, BypassHostCacheOnRefresh3) {
  BypassHostCacheOnRefreshHelper(LOAD_DISABLE_CACHE);
}

// Make sure we can handle an error when writing the request.
TEST_P(HttpNetworkTransactionTest, RequestWriteError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockWrite write_failure[] = {
    MockWrite(ASYNC, ERR_CONNECTION_RESET),
  };
  StaticSocketDataProvider data(NULL, 0,
                                write_failure, arraysize(write_failure));
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
}

// Check that a connection closed after the start of the headers finishes ok.
TEST_P(HttpNetworkTransactionTest, ConnectionClosedAfterStartOfHeaders) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1."),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("", response_data);
}

// Make sure that a dropped connection while draining the body for auth
// restart does the right thing.
TEST_P(HttpNetworkTransactionTest, DrainResetOK) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 14\r\n\r\n"),
    MockRead("Unauth"),
    MockRead(ASYNC, ERR_CONNECTION_RESET),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  // After calling trans->RestartWithAuth(), this is the request we should
  // be issuing -- the final header line contains the credentials.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  session_deps_.socket_factory->AddSocketDataProvider(&data2);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(CheckBasicServerAuth(response->auth_challenge.get()));

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test HTTPS connections going through a proxy that sends extra data.
TEST_P(HttpNetworkTransactionTest, HTTPSViaProxyWithExtraData) {
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockRead proxy_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\nExtra data"),
    MockRead(SYNCHRONOUS, OK)
  };

  StaticSocketDataProvider data(proxy_reads, arraysize(proxy_reads), NULL, 0);
  SSLSocketDataProvider ssl(ASYNC, OK);

  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  session_deps_.socket_factory->ResetNextMockIndexes();

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);
}

TEST_P(HttpNetworkTransactionTest, LargeContentLengthThenClose) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\nContent-Length:6719476739\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, rv);
}

TEST_P(HttpNetworkTransactionTest, UploadFileSmallerThanLength) {
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  const uint64 kFakeSize = 100000;  // file is actually blank
  UploadFileElementReader::ScopedOverridingContentLengthForTests
      overriding_content_length(kFakeSize);

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(
      new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                  temp_file_path,
                                  0,
                                  kuint64max,
                                  base::Time()));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/upload");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);

  base::DeleteFile(temp_file_path, false);
}

TEST_P(HttpNetworkTransactionTest, UploadUnreadableFile) {
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file));
  std::string temp_file_content("Unreadable file.");
  ASSERT_TRUE(base::WriteFile(temp_file, temp_file_content.c_str(),
                                   temp_file_content.length()));
  ASSERT_TRUE(file_util::MakeFileUnreadable(temp_file));

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(
      new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                  temp_file,
                                  0,
                                  kuint64max,
                                  base::Time()));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/upload");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  // If we try to upload an unreadable file, the transaction should fail.
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  StaticSocketDataProvider data(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_ACCESS_DENIED, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response);

  base::DeleteFile(temp_file, false);
}

TEST_P(HttpNetworkTransactionTest, CancelDuringInitRequestBody) {
  class FakeUploadElementReader : public UploadElementReader {
   public:
    FakeUploadElementReader() {}
    virtual ~FakeUploadElementReader() {}

    const CompletionCallback& callback() const { return callback_; }

    // UploadElementReader overrides:
    virtual int Init(const CompletionCallback& callback) OVERRIDE {
      callback_ = callback;
      return ERR_IO_PENDING;
    }
    virtual uint64 GetContentLength() const OVERRIDE { return 0; }
    virtual uint64 BytesRemaining() const OVERRIDE { return 0; }
    virtual int Read(IOBuffer* buf,
                     int buf_length,
                     const CompletionCallback& callback) OVERRIDE {
      return ERR_FAILED;
    }

   private:
    CompletionCallback callback_;
  };

  FakeUploadElementReader* fake_reader = new FakeUploadElementReader;
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(fake_reader);
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/upload");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  StaticSocketDataProvider data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;
  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  base::MessageLoop::current()->RunUntilIdle();

  // Transaction is pending on request body initialization.
  ASSERT_FALSE(fake_reader->callback().is_null());

  // Return Init()'s result after the transaction gets destroyed.
  trans.reset();
  fake_reader->callback().Run(OK);  // Should not crash.
}

// Tests that changes to Auth realms are treated like auth rejections.
TEST_P(HttpNetworkTransactionTest, ChangeAuthRealms) {

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // First transaction will request a resource and receive a Basic challenge
  // with realm="first_realm".
  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "\r\n"),
  };
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"
             "WWW-Authenticate: Basic realm=\"first_realm\"\r\n"
             "\r\n"),
  };

  // After calling trans->RestartWithAuth(), provide an Authentication header
  // for first_realm. The server will reject and provide a challenge with
  // second_realm.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zmlyc3Q6YmF6\r\n"
              "\r\n"),
  };
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"
             "WWW-Authenticate: Basic realm=\"second_realm\"\r\n"
             "\r\n"),
  };

  // This again fails, and goes back to first_realm. Make sure that the
  // entry is removed from cache.
  MockWrite data_writes3[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic c2Vjb25kOmZvdQ==\r\n"
              "\r\n"),
  };
  MockRead data_reads3[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"
             "WWW-Authenticate: Basic realm=\"first_realm\"\r\n"
             "\r\n"),
  };

  // Try one last time (with the correct password) and get the resource.
  MockWrite data_writes4[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zmlyc3Q6YmFy\r\n"
              "\r\n"),
  };
  MockRead data_reads4[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=iso-8859-1\r\n"
             "Content-Length: 5\r\n"
             "\r\n"
             "hello"),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  StaticSocketDataProvider data3(data_reads3, arraysize(data_reads3),
                                 data_writes3, arraysize(data_writes3));
  StaticSocketDataProvider data4(data_reads4, arraysize(data_reads4),
                                 data_writes4, arraysize(data_writes4));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);
  session_deps_.socket_factory->AddSocketDataProvider(&data4);

  TestCompletionCallback callback1;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  // Issue the first request with Authorize headers. There should be a
  // password prompt for first_realm waiting to be filled in after the
  // transaction completes.
  int rv = trans->Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  const AuthChallengeInfo* challenge = response->auth_challenge.get();
  ASSERT_FALSE(challenge == NULL);
  EXPECT_FALSE(challenge->is_proxy);
  EXPECT_EQ("www.google.com:80", challenge->challenger.ToString());
  EXPECT_EQ("first_realm", challenge->realm);
  EXPECT_EQ("basic", challenge->scheme);

  // Issue the second request with an incorrect password. There should be a
  // password prompt for second_realm waiting to be filled in after the
  // transaction completes.
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(
      AuthCredentials(kFirst, kBaz), callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  challenge = response->auth_challenge.get();
  ASSERT_FALSE(challenge == NULL);
  EXPECT_FALSE(challenge->is_proxy);
  EXPECT_EQ("www.google.com:80", challenge->challenger.ToString());
  EXPECT_EQ("second_realm", challenge->realm);
  EXPECT_EQ("basic", challenge->scheme);

  // Issue the third request with another incorrect password. There should be
  // a password prompt for first_realm waiting to be filled in. If the password
  // prompt is not present, it indicates that the HttpAuthCacheEntry for
  // first_realm was not correctly removed.
  TestCompletionCallback callback3;
  rv = trans->RestartWithAuth(
      AuthCredentials(kSecond, kFou), callback3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  challenge = response->auth_challenge.get();
  ASSERT_FALSE(challenge == NULL);
  EXPECT_FALSE(challenge->is_proxy);
  EXPECT_EQ("www.google.com:80", challenge->challenger.ToString());
  EXPECT_EQ("first_realm", challenge->realm);
  EXPECT_EQ("basic", challenge->scheme);

  // Issue the fourth request with the correct password and username.
  TestCompletionCallback callback4;
  rv = trans->RestartWithAuth(
      AuthCredentials(kFirst, kBar), callback4.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback4.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
}

TEST_P(HttpNetworkTransactionTest, HonorAlternateProtocolHeader) {
  session_deps_.next_protos = SpdyNextProtos();
  session_deps_.use_alternate_protocols = true;

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  HostPortPair http_host_port_pair("www.google.com", 80);
  HttpServerProperties& http_server_properties =
      *session->http_server_properties();
  EXPECT_FALSE(
      http_server_properties.HasAlternateProtocol(http_host_port_pair));

  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_FALSE(response->was_npn_negotiated);

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  ASSERT_TRUE(http_server_properties.HasAlternateProtocol(http_host_port_pair));
  const PortAlternateProtocolPair alternate =
      http_server_properties.GetAlternateProtocol(http_host_port_pair);
  PortAlternateProtocolPair expected_alternate;
  expected_alternate.port = 443;
  expected_alternate.protocol = AlternateProtocolFromNextProto(GetParam());
  EXPECT_TRUE(expected_alternate.Equals(alternate));
}

TEST_P(HttpNetworkTransactionTest,
       MarkBrokenAlternateProtocolAndFallback) {
  session_deps_.use_alternate_protocols = true;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider first_data;
  first_data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&first_data);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider second_data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&second_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  // Port must be < 1024, or the header will be ignored (since initial port was
  // port 80 (another restricted port).
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(request.url),
      666 /* port is ignored by MockConnect anyway */,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  ASSERT_TRUE(http_server_properties->HasAlternateProtocol(
      HostPortPair::FromURL(request.url)));
  const PortAlternateProtocolPair alternate =
      http_server_properties->GetAlternateProtocol(
          HostPortPair::FromURL(request.url));
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol);
}

TEST_P(HttpNetworkTransactionTest,
       AlternateProtocolPortRestrictedBlocked) {
  // Ensure that we're not allowed to redirect traffic via an alternate
  // protocol to an unrestricted (port >= 1024) when the original traffic was
  // on a restricted port (port < 1024).  Ensure that we can redirect in all
  // other cases.
  session_deps_.use_alternate_protocols = true;

  HttpRequestInfo restricted_port_request;
  restricted_port_request.method = "GET";
  restricted_port_request.url = GURL("http://www.google.com:1023/");
  restricted_port_request.load_flags = 0;

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider first_data;
  first_data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&first_data);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider second_data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&second_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  const int kUnrestrictedAlternatePort = 1024;
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(restricted_port_request.url),
      kUnrestrictedAlternatePort,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  int rv = trans->Start(
      &restricted_port_request,
      callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Invalid change to unrestricted port should fail.
  EXPECT_EQ(ERR_CONNECTION_REFUSED, callback.WaitForResult());
}

TEST_P(HttpNetworkTransactionTest,
       AlternateProtocolPortRestrictedPermitted) {
  // Ensure that we're allowed to redirect traffic via an alternate
  // protocol to an unrestricted (port >= 1024) when the original traffic was
  // on a restricted port (port < 1024) if we set
  // enable_user_alternate_protocol_ports.

  session_deps_.use_alternate_protocols = true;
  session_deps_.enable_user_alternate_protocol_ports = true;

  HttpRequestInfo restricted_port_request;
  restricted_port_request.method = "GET";
  restricted_port_request.url = GURL("http://www.google.com:1023/");
  restricted_port_request.load_flags = 0;

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider first_data;
  first_data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&first_data);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider second_data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&second_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  const int kUnrestrictedAlternatePort = 1024;
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(restricted_port_request.url),
      kUnrestrictedAlternatePort,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  EXPECT_EQ(ERR_IO_PENDING, trans->Start(
      &restricted_port_request,
      callback.callback(), BoundNetLog()));
  // Change to unrestricted port should succeed.
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_P(HttpNetworkTransactionTest,
       AlternateProtocolPortRestrictedAllowed) {
  // Ensure that we're not allowed to redirect traffic via an alternate
  // protocol to an unrestricted (port >= 1024) when the original traffic was
  // on a restricted port (port < 1024).  Ensure that we can redirect in all
  // other cases.
  session_deps_.use_alternate_protocols = true;

  HttpRequestInfo restricted_port_request;
  restricted_port_request.method = "GET";
  restricted_port_request.url = GURL("http://www.google.com:1023/");
  restricted_port_request.load_flags = 0;

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider first_data;
  first_data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&first_data);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider second_data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&second_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  const int kRestrictedAlternatePort = 80;
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(restricted_port_request.url),
      kRestrictedAlternatePort,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  int rv = trans->Start(
      &restricted_port_request,
      callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Valid change to restricted port should pass.
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_P(HttpNetworkTransactionTest,
       AlternateProtocolPortUnrestrictedAllowed1) {
  // Ensure that we're not allowed to redirect traffic via an alternate
  // protocol to an unrestricted (port >= 1024) when the original traffic was
  // on a restricted port (port < 1024).  Ensure that we can redirect in all
  // other cases.
  session_deps_.use_alternate_protocols = true;

  HttpRequestInfo unrestricted_port_request;
  unrestricted_port_request.method = "GET";
  unrestricted_port_request.url = GURL("http://www.google.com:1024/");
  unrestricted_port_request.load_flags = 0;

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider first_data;
  first_data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&first_data);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider second_data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&second_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  const int kRestrictedAlternatePort = 80;
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(unrestricted_port_request.url),
      kRestrictedAlternatePort,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  int rv = trans->Start(
      &unrestricted_port_request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Valid change to restricted port should pass.
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_P(HttpNetworkTransactionTest,
       AlternateProtocolPortUnrestrictedAllowed2) {
  // Ensure that we're not allowed to redirect traffic via an alternate
  // protocol to an unrestricted (port >= 1024) when the original traffic was
  // on a restricted port (port < 1024).  Ensure that we can redirect in all
  // other cases.
  session_deps_.use_alternate_protocols = true;

  HttpRequestInfo unrestricted_port_request;
  unrestricted_port_request.method = "GET";
  unrestricted_port_request.url = GURL("http://www.google.com:1024/");
  unrestricted_port_request.load_flags = 0;

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider first_data;
  first_data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&first_data);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider second_data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&second_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  const int kUnrestrictedAlternatePort = 1024;
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(unrestricted_port_request.url),
      kUnrestrictedAlternatePort,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  int rv = trans->Start(
      &unrestricted_port_request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Valid change to an unrestricted port should pass.
  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_P(HttpNetworkTransactionTest, AlternateProtocolUnsafeBlocked) {
  // Ensure that we're not allowed to redirect traffic via an alternate
  // protocol to an unsafe port, and that we resume the second
  // HttpStreamFactoryImpl::Job once the alternate protocol request fails.
  session_deps_.use_alternate_protocols = true;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // The alternate protocol request will error out before we attempt to connect,
  // so only the standard HTTP request will try to connect.
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };
  StaticSocketDataProvider data(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  base::WeakPtr<HttpServerProperties> http_server_properties =
      session->http_server_properties();
  const int kUnsafePort = 7;
  http_server_properties->SetAlternateProtocol(
      HostPortPair::FromURL(request.url),
      kUnsafePort,
      AlternateProtocolFromNextProto(GetParam()));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // The HTTP request should succeed.
  EXPECT_EQ(OK, callback.WaitForResult());

  // Disable alternate protocol before the asserts.
 // HttpStreamFactory::set_use_alternate_protocols(false);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);
}

TEST_P(HttpNetworkTransactionTest, UseAlternateProtocolForNpnSpdy) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&first_transaction);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*data),
    MockRead(ASYNC, 0, 0),
  };

  DelayedSocketData spdy_data(
      1,  // wait for one write to finish before reading.
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_non_alternate_protocol_socket(
      NULL, 0, NULL, 0);
  hanging_non_alternate_protocol_socket.set_connect_data(
      never_finishing_connect);
  session_deps_.socket_factory->AddSocketDataProvider(
      &hanging_non_alternate_protocol_socket);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);

  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
}

TEST_P(HttpNetworkTransactionTest, AlternateProtocolWithSpdyLateBinding) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK),
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  // Socket 1 is the HTTP transaction with the Alternate-Protocol header.
  session_deps_.socket_factory->AddSocketDataProvider(&first_transaction);

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_socket(
      NULL, 0, NULL, 0);
  hanging_socket.set_connect_data(never_finishing_connect);
  // Socket 2 and 3 are the hanging Alternate-Protocol and
  // non-Alternate-Protocol jobs from the 2nd transaction.
  session_deps_.socket_factory->AddSocketDataProvider(&hanging_socket);
  session_deps_.socket_factory->AddSocketDataProvider(&hanging_socket);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req1),
    CreateMockWrite(*req2),
  };
  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> data2(spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp1),
    CreateMockRead(*data1),
    CreateMockRead(*resp2),
    CreateMockRead(*data2),
    MockRead(ASYNC, 0, 0),
  };

  DelayedSocketData spdy_data(
      2,  // wait for writes to finish before reading.
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  // Socket 4 is the successful Alternate-Protocol for transaction 3.
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  // Socket 5 is the unsuccessful non-Alternate-Protocol for transaction 3.
  session_deps_.socket_factory->AddSocketDataProvider(&hanging_socket);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  TestCompletionCallback callback1;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session.get());

  int rv = trans1.Start(&request, callback1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback1.WaitForResult());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(&trans1, &response_data));
  EXPECT_EQ("hello world", response_data);

  TestCompletionCallback callback2;
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session.get());
  rv = trans2.Start(&request, callback2.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestCompletionCallback callback3;
  HttpNetworkTransaction trans3(DEFAULT_PRIORITY, session.get());
  rv = trans3.Start(&request, callback3.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_EQ(OK, callback3.WaitForResult());

  response = trans2.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(&trans2, &response_data));
  EXPECT_EQ("hello!", response_data);

  response = trans3.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(&trans3, &response_data));
  EXPECT_EQ("hello!", response_data);
}

TEST_P(HttpNetworkTransactionTest, StallAlternateProtocolForNpnSpdy) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK),
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&first_transaction);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_alternate_protocol_socket(
      NULL, 0, NULL, 0);
  hanging_alternate_protocol_socket.set_connect_data(
      never_finishing_connect);
  session_deps_.socket_factory->AddSocketDataProvider(
      &hanging_alternate_protocol_socket);

  // 2nd request is just a copy of the first one, over HTTP again.
  session_deps_.socket_factory->AddSocketDataProvider(&first_transaction);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_FALSE(response->was_npn_negotiated);

  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);
}

class CapturingProxyResolver : public ProxyResolver {
 public:
  CapturingProxyResolver() : ProxyResolver(false /* expects_pac_bytes */) {}
  virtual ~CapturingProxyResolver() {}

  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             const CompletionCallback& callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE {
    ProxyServer proxy_server(ProxyServer::SCHEME_HTTP,
                             HostPortPair("myproxy", 80));
    results->UseProxyServer(proxy_server);
    resolved_.push_back(url);
    return OK;
  }

  virtual void CancelRequest(RequestHandle request) OVERRIDE {
    NOTREACHED();
  }

  virtual LoadState GetLoadState(RequestHandle request) const OVERRIDE {
    NOTREACHED();
    return LOAD_STATE_IDLE;
  }

  virtual void CancelSetPacScript() OVERRIDE {
    NOTREACHED();
  }

  virtual int SetPacScript(const scoped_refptr<ProxyResolverScriptData>&,
                           const CompletionCallback& /*callback*/) OVERRIDE {
    return OK;
  }

  const std::vector<GURL>& resolved() const { return resolved_; }

 private:
  std::vector<GURL> resolved_;

  DISALLOW_COPY_AND_ASSIGN(CapturingProxyResolver);
};

TEST_P(HttpNetworkTransactionTest,
       UseAlternateProtocolForTunneledNpnSpdy) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  ProxyConfig proxy_config;
  proxy_config.set_auto_detect(true);
  proxy_config.set_pac_url(GURL("http://fooproxyurl"));

  CapturingProxyResolver* capturing_proxy_resolver =
      new CapturingProxyResolver();
  session_deps_.proxy_service.reset(new ProxyService(
      new ProxyConfigServiceFixed(proxy_config), capturing_proxy_resolver,
      NULL));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK),
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&first_transaction);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite spdy_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),  // 0
    CreateMockWrite(*req),                              // 3
  };

  const char kCONNECTResponse[] = "HTTP/1.1 200 Connected\r\n\r\n";

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    MockRead(ASYNC, kCONNECTResponse, arraysize(kCONNECTResponse) - 1, 1),  // 1
    CreateMockRead(*resp.get(), 4),  // 2, 4
    CreateMockRead(*data.get(), 4),  // 5
    MockRead(ASYNC, 0, 0, 4),  // 6
  };

  OrderedSocketData spdy_data(
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_non_alternate_protocol_socket(
      NULL, 0, NULL, 0);
  hanging_non_alternate_protocol_socket.set_connect_data(
      never_finishing_connect);
  session_deps_.socket_factory->AddSocketDataProvider(
      &hanging_non_alternate_protocol_socket);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_FALSE(response->was_npn_negotiated);

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);

  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
  ASSERT_EQ(3u, capturing_proxy_resolver->resolved().size());
  EXPECT_EQ("http://www.google.com/",
            capturing_proxy_resolver->resolved()[0].spec());
  EXPECT_EQ("https://www.google.com/",
            capturing_proxy_resolver->resolved()[1].spec());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);
}

TEST_P(HttpNetworkTransactionTest,
       UseAlternateProtocolForNpnSpdyWithExistingSpdySession) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(ASYNC, OK),
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&first_transaction);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*data),
    MockRead(ASYNC, 0, 0),
  };

  DelayedSocketData spdy_data(
      1,  // wait for one write to finish before reading.
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  // Set up an initial SpdySession in the pool to reuse.
  HostPortPair host_port_pair("www.google.com", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> spdy_session =
      CreateSecureSpdySession(session, key, BoundNetLog());

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);

  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
}

// GenerateAuthToken is a mighty big test.
// It tests all permutation of GenerateAuthToken behavior:
//   - Synchronous and Asynchronous completion.
//   - OK or error on completion.
//   - Direct connection, non-authenticating proxy, and authenticating proxy.
//   - HTTP or HTTPS backend (to include proxy tunneling).
//   - Non-authenticating and authenticating backend.
//
// In all, there are 44 reasonable permuations (for example, if there are
// problems generating an auth token for an authenticating proxy, we don't
// need to test all permutations of the backend server).
//
// The test proceeds by going over each of the configuration cases, and
// potentially running up to three rounds in each of the tests. The TestConfig
// specifies both the configuration for the test as well as the expectations
// for the results.
TEST_P(HttpNetworkTransactionTest, GenerateAuthToken) {
  static const char kServer[] = "http://www.example.com";
  static const char kSecureServer[] = "https://www.example.com";
  static const char kProxy[] = "myproxy:70";
  const int kAuthErr = ERR_INVALID_AUTH_CREDENTIALS;

  enum AuthTiming {
    AUTH_NONE,
    AUTH_SYNC,
    AUTH_ASYNC,
  };

  const MockWrite kGet(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n\r\n");
  const MockWrite kGetProxy(
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n\r\n");
  const MockWrite kGetAuth(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n"
      "Authorization: auth_token\r\n\r\n");
  const MockWrite kGetProxyAuth(
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: auth_token\r\n\r\n");
  const MockWrite kGetAuthThroughProxy(
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Authorization: auth_token\r\n\r\n");
  const MockWrite kGetAuthWithProxyAuth(
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: auth_token\r\n"
      "Authorization: auth_token\r\n\r\n");
  const MockWrite kConnect(
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n\r\n");
  const MockWrite kConnectProxyAuth(
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: auth_token\r\n\r\n");

  const MockRead kSuccess(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n"
      "Content-Length: 3\r\n\r\n"
      "Yes");
  const MockRead kFailure(
      "Should not be called.");
  const MockRead kServerChallenge(
      "HTTP/1.1 401 Unauthorized\r\n"
      "WWW-Authenticate: Mock realm=server\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n"
      "Content-Length: 14\r\n\r\n"
      "Unauthorized\r\n");
  const MockRead kProxyChallenge(
      "HTTP/1.1 407 Unauthorized\r\n"
      "Proxy-Authenticate: Mock realm=proxy\r\n"
      "Proxy-Connection: close\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n"
      "Content-Length: 14\r\n\r\n"
      "Unauthorized\r\n");
  const MockRead kProxyConnected(
      "HTTP/1.1 200 Connection Established\r\n\r\n");

  // NOTE(cbentzel): I wanted TestReadWriteRound to be a simple struct with
  // no constructors, but the C++ compiler on Windows warns about
  // unspecified data in compound literals. So, moved to using constructors,
  // and TestRound's created with the default constructor should not be used.
  struct TestRound {
    TestRound()
        : expected_rv(ERR_UNEXPECTED),
          extra_write(NULL),
          extra_read(NULL) {
    }
    TestRound(const MockWrite& write_arg, const MockRead& read_arg,
              int expected_rv_arg)
        : write(write_arg),
          read(read_arg),
          expected_rv(expected_rv_arg),
          extra_write(NULL),
          extra_read(NULL) {
    }
    TestRound(const MockWrite& write_arg, const MockRead& read_arg,
              int expected_rv_arg, const MockWrite* extra_write_arg,
              const MockRead* extra_read_arg)
        : write(write_arg),
          read(read_arg),
          expected_rv(expected_rv_arg),
          extra_write(extra_write_arg),
          extra_read(extra_read_arg) {
    }
    MockWrite write;
    MockRead read;
    int expected_rv;
    const MockWrite* extra_write;
    const MockRead* extra_read;
  };

  static const int kNoSSL = 500;

  struct TestConfig {
    const char* proxy_url;
    AuthTiming proxy_auth_timing;
    int proxy_auth_rv;
    const char* server_url;
    AuthTiming server_auth_timing;
    int server_auth_rv;
    int num_auth_rounds;
    int first_ssl_round;
    TestRound rounds[3];
  } test_configs[] = {
    // Non-authenticating HTTP server with a direct connection.
    { NULL, AUTH_NONE, OK, kServer, AUTH_NONE, OK, 1, kNoSSL,
      { TestRound(kGet, kSuccess, OK)}},
    // Authenticating HTTP server with a direct connection.
    { NULL, AUTH_NONE, OK, kServer, AUTH_SYNC, OK, 2, kNoSSL,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kSuccess, OK)}},
    { NULL, AUTH_NONE, OK, kServer, AUTH_SYNC, kAuthErr, 2, kNoSSL,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    { NULL, AUTH_NONE, OK, kServer, AUTH_ASYNC, OK, 2, kNoSSL,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kSuccess, OK)}},
    { NULL, AUTH_NONE, OK, kServer, AUTH_ASYNC, kAuthErr, 2, kNoSSL,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    // Non-authenticating HTTP server through a non-authenticating proxy.
    { kProxy, AUTH_NONE, OK, kServer, AUTH_NONE, OK, 1, kNoSSL,
      { TestRound(kGetProxy, kSuccess, OK)}},
    // Authenticating HTTP server through a non-authenticating proxy.
    { kProxy, AUTH_NONE, OK, kServer, AUTH_SYNC, OK, 2, kNoSSL,
      { TestRound(kGetProxy, kServerChallenge, OK),
        TestRound(kGetAuthThroughProxy, kSuccess, OK)}},
    { kProxy, AUTH_NONE, OK, kServer, AUTH_SYNC, kAuthErr, 2, kNoSSL,
      { TestRound(kGetProxy, kServerChallenge, OK),
        TestRound(kGetAuthThroughProxy, kFailure, kAuthErr)}},
    { kProxy, AUTH_NONE, OK, kServer, AUTH_ASYNC, OK, 2, kNoSSL,
      { TestRound(kGetProxy, kServerChallenge, OK),
        TestRound(kGetAuthThroughProxy, kSuccess, OK)}},
    { kProxy, AUTH_NONE, OK, kServer, AUTH_ASYNC, kAuthErr, 2, kNoSSL,
      { TestRound(kGetProxy, kServerChallenge, OK),
        TestRound(kGetAuthThroughProxy, kFailure, kAuthErr)}},
    // Non-authenticating HTTP server through an authenticating proxy.
    { kProxy, AUTH_SYNC, OK, kServer, AUTH_NONE, OK, 2, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kSuccess, OK)}},
    { kProxy, AUTH_SYNC, kAuthErr, kServer, AUTH_NONE, OK, 2, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_ASYNC, OK, kServer, AUTH_NONE, OK, 2, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kSuccess, OK)}},
    { kProxy, AUTH_ASYNC, kAuthErr, kServer, AUTH_NONE, OK, 2, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kFailure, kAuthErr)}},
    // Authenticating HTTP server through an authenticating proxy.
    { kProxy, AUTH_SYNC, OK, kServer, AUTH_SYNC, OK, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kSuccess, OK)}},
    { kProxy, AUTH_SYNC, OK, kServer, AUTH_SYNC, kAuthErr, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_ASYNC, OK, kServer, AUTH_SYNC, OK, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kSuccess, OK)}},
    { kProxy, AUTH_ASYNC, OK, kServer, AUTH_SYNC, kAuthErr, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_SYNC, OK, kServer, AUTH_ASYNC, OK, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kSuccess, OK)}},
    { kProxy, AUTH_SYNC, OK, kServer, AUTH_ASYNC, kAuthErr, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_ASYNC, OK, kServer, AUTH_ASYNC, OK, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kSuccess, OK)}},
    { kProxy, AUTH_ASYNC, OK, kServer, AUTH_ASYNC, kAuthErr, 3, kNoSSL,
      { TestRound(kGetProxy, kProxyChallenge, OK),
        TestRound(kGetProxyAuth, kServerChallenge, OK),
        TestRound(kGetAuthWithProxyAuth, kFailure, kAuthErr)}},
    // Non-authenticating HTTPS server with a direct connection.
    { NULL, AUTH_NONE, OK, kSecureServer, AUTH_NONE, OK, 1, 0,
      { TestRound(kGet, kSuccess, OK)}},
    // Authenticating HTTPS server with a direct connection.
    { NULL, AUTH_NONE, OK, kSecureServer, AUTH_SYNC, OK, 2, 0,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kSuccess, OK)}},
    { NULL, AUTH_NONE, OK, kSecureServer, AUTH_SYNC, kAuthErr, 2, 0,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    { NULL, AUTH_NONE, OK, kSecureServer, AUTH_ASYNC, OK, 2, 0,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kSuccess, OK)}},
    { NULL, AUTH_NONE, OK, kSecureServer, AUTH_ASYNC, kAuthErr, 2, 0,
      { TestRound(kGet, kServerChallenge, OK),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    // Non-authenticating HTTPS server with a non-authenticating proxy.
    { kProxy, AUTH_NONE, OK, kSecureServer, AUTH_NONE, OK, 1, 0,
      { TestRound(kConnect, kProxyConnected, OK, &kGet, &kSuccess)}},
    // Authenticating HTTPS server through a non-authenticating proxy.
    { kProxy, AUTH_NONE, OK, kSecureServer, AUTH_SYNC, OK, 2, 0,
      { TestRound(kConnect, kProxyConnected, OK, &kGet, &kServerChallenge),
        TestRound(kGetAuth, kSuccess, OK)}},
    { kProxy, AUTH_NONE, OK, kSecureServer, AUTH_SYNC, kAuthErr, 2, 0,
      { TestRound(kConnect, kProxyConnected, OK, &kGet, &kServerChallenge),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_NONE, OK, kSecureServer, AUTH_ASYNC, OK, 2, 0,
      { TestRound(kConnect, kProxyConnected, OK, &kGet, &kServerChallenge),
        TestRound(kGetAuth, kSuccess, OK)}},
    { kProxy, AUTH_NONE, OK, kSecureServer, AUTH_ASYNC, kAuthErr, 2, 0,
      { TestRound(kConnect, kProxyConnected, OK, &kGet, &kServerChallenge),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    // Non-Authenticating HTTPS server through an authenticating proxy.
    { kProxy, AUTH_SYNC, OK, kSecureServer, AUTH_NONE, OK, 2, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK, &kGet, &kSuccess)}},
    { kProxy, AUTH_SYNC, kAuthErr, kSecureServer, AUTH_NONE, OK, 2, kNoSSL,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_ASYNC, OK, kSecureServer, AUTH_NONE, OK, 2, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK, &kGet, &kSuccess)}},
    { kProxy, AUTH_ASYNC, kAuthErr, kSecureServer, AUTH_NONE, OK, 2, kNoSSL,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kFailure, kAuthErr)}},
    // Authenticating HTTPS server through an authenticating proxy.
    { kProxy, AUTH_SYNC, OK, kSecureServer, AUTH_SYNC, OK, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kSuccess, OK)}},
    { kProxy, AUTH_SYNC, OK, kSecureServer, AUTH_SYNC, kAuthErr, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_ASYNC, OK, kSecureServer, AUTH_SYNC, OK, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kSuccess, OK)}},
    { kProxy, AUTH_ASYNC, OK, kSecureServer, AUTH_SYNC, kAuthErr, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_SYNC, OK, kSecureServer, AUTH_ASYNC, OK, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kSuccess, OK)}},
    { kProxy, AUTH_SYNC, OK, kSecureServer, AUTH_ASYNC, kAuthErr, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
    { kProxy, AUTH_ASYNC, OK, kSecureServer, AUTH_ASYNC, OK, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kSuccess, OK)}},
    { kProxy, AUTH_ASYNC, OK, kSecureServer, AUTH_ASYNC, kAuthErr, 3, 1,
      { TestRound(kConnect, kProxyChallenge, OK),
        TestRound(kConnectProxyAuth, kProxyConnected, OK,
                  &kGet, &kServerChallenge),
        TestRound(kGetAuth, kFailure, kAuthErr)}},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_configs); ++i) {
    HttpAuthHandlerMock::Factory* auth_factory(
        new HttpAuthHandlerMock::Factory());
    session_deps_.http_auth_handler_factory.reset(auth_factory);
    const TestConfig& test_config = test_configs[i];

    // Set up authentication handlers as necessary.
    if (test_config.proxy_auth_timing != AUTH_NONE) {
      for (int n = 0; n < 2; n++) {
        HttpAuthHandlerMock* auth_handler(new HttpAuthHandlerMock());
        std::string auth_challenge = "Mock realm=proxy";
        GURL origin(test_config.proxy_url);
        HttpAuthChallengeTokenizer tokenizer(auth_challenge.begin(),
                                             auth_challenge.end());
        auth_handler->InitFromChallenge(&tokenizer, HttpAuth::AUTH_PROXY,
                                        origin, BoundNetLog());
        auth_handler->SetGenerateExpectation(
            test_config.proxy_auth_timing == AUTH_ASYNC,
            test_config.proxy_auth_rv);
        auth_factory->AddMockHandler(auth_handler, HttpAuth::AUTH_PROXY);
      }
    }
    if (test_config.server_auth_timing != AUTH_NONE) {
      HttpAuthHandlerMock* auth_handler(new HttpAuthHandlerMock());
      std::string auth_challenge = "Mock realm=server";
      GURL origin(test_config.server_url);
      HttpAuthChallengeTokenizer tokenizer(auth_challenge.begin(),
                                           auth_challenge.end());
      auth_handler->InitFromChallenge(&tokenizer, HttpAuth::AUTH_SERVER,
                                      origin, BoundNetLog());
      auth_handler->SetGenerateExpectation(
          test_config.server_auth_timing == AUTH_ASYNC,
          test_config.server_auth_rv);
      auth_factory->AddMockHandler(auth_handler, HttpAuth::AUTH_SERVER);
    }
    if (test_config.proxy_url) {
      session_deps_.proxy_service.reset(
          ProxyService::CreateFixed(test_config.proxy_url));
    } else {
      session_deps_.proxy_service.reset(ProxyService::CreateDirect());
    }

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL(test_config.server_url);
    request.load_flags = 0;

    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session);

    for (int round = 0; round < test_config.num_auth_rounds; ++round) {
      const TestRound& read_write_round = test_config.rounds[round];

      // Set up expected reads and writes.
      MockRead reads[2];
      reads[0] = read_write_round.read;
      size_t length_reads = 1;
      if (read_write_round.extra_read) {
        reads[1] = *read_write_round.extra_read;
        length_reads = 2;
      }

      MockWrite writes[2];
      writes[0] = read_write_round.write;
      size_t length_writes = 1;
      if (read_write_round.extra_write) {
        writes[1] = *read_write_round.extra_write;
        length_writes = 2;
      }
      StaticSocketDataProvider data_provider(
          reads, length_reads, writes, length_writes);
      session_deps_.socket_factory->AddSocketDataProvider(&data_provider);

      // Add an SSL sequence if necessary.
      SSLSocketDataProvider ssl_socket_data_provider(SYNCHRONOUS, OK);
      if (round >= test_config.first_ssl_round)
        session_deps_.socket_factory->AddSSLSocketDataProvider(
            &ssl_socket_data_provider);

      // Start or restart the transaction.
      TestCompletionCallback callback;
      int rv;
      if (round == 0) {
        rv = trans.Start(&request, callback.callback(), BoundNetLog());
      } else {
        rv = trans.RestartWithAuth(
            AuthCredentials(kFoo, kBar), callback.callback());
      }
      if (rv == ERR_IO_PENDING)
        rv = callback.WaitForResult();

      // Compare results with expected data.
      EXPECT_EQ(read_write_round.expected_rv, rv);
      const HttpResponseInfo* response = trans.GetResponseInfo();
      if (read_write_round.expected_rv == OK) {
        ASSERT_TRUE(response != NULL);
      } else {
        EXPECT_TRUE(response == NULL);
        EXPECT_EQ(round + 1, test_config.num_auth_rounds);
        continue;
      }
      if (round + 1 < test_config.num_auth_rounds) {
        EXPECT_FALSE(response->auth_challenge.get() == NULL);
      } else {
        EXPECT_TRUE(response->auth_challenge.get() == NULL);
      }
    }
  }
}

TEST_P(HttpNetworkTransactionTest, MultiRoundAuth) {
  // Do multi-round authentication and make sure it works correctly.
  HttpAuthHandlerMock::Factory* auth_factory(
      new HttpAuthHandlerMock::Factory());
  session_deps_.http_auth_handler_factory.reset(auth_factory);
  session_deps_.proxy_service.reset(ProxyService::CreateDirect());
  session_deps_.host_resolver->rules()->AddRule("www.example.com", "10.0.0.1");
  session_deps_.host_resolver->set_synchronous_mode(true);

  HttpAuthHandlerMock* auth_handler(new HttpAuthHandlerMock());
  auth_handler->set_connection_based(true);
  std::string auth_challenge = "Mock realm=server";
  GURL origin("http://www.example.com");
  HttpAuthChallengeTokenizer tokenizer(auth_challenge.begin(),
                                       auth_challenge.end());
  auth_handler->InitFromChallenge(&tokenizer, HttpAuth::AUTH_SERVER,
                                  origin, BoundNetLog());
  auth_factory->AddMockHandler(auth_handler, HttpAuth::AUTH_SERVER);

  int rv = OK;
  const HttpResponseInfo* response = NULL;
  HttpRequestInfo request;
  request.method = "GET";
  request.url = origin;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Use a TCP Socket Pool with only one connection per group. This is used
  // to validate that the TCP socket is not released to the pool between
  // each round of multi-round authentication.
  HttpNetworkSessionPeer session_peer(session);
  ClientSocketPoolHistograms transport_pool_histograms("SmallTCP");
  TransportClientSocketPool* transport_pool = new TransportClientSocketPool(
      50,  // Max sockets for pool
      1,   // Max sockets per group
      &transport_pool_histograms,
      session_deps_.host_resolver.get(),
      session_deps_.socket_factory.get(),
      session_deps_.net_log);
  scoped_ptr<MockClientSocketPoolManager> mock_pool_manager(
      new MockClientSocketPoolManager);
  mock_pool_manager->SetTransportSocketPool(transport_pool);
  session_peer.SetClientSocketPoolManager(
      mock_pool_manager.PassAs<ClientSocketPoolManager>());

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback;

  const MockWrite kGet(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n\r\n");
  const MockWrite kGetAuth(
      "GET / HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Connection: keep-alive\r\n"
      "Authorization: auth_token\r\n\r\n");

  const MockRead kServerChallenge(
      "HTTP/1.1 401 Unauthorized\r\n"
      "WWW-Authenticate: Mock realm=server\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n"
      "Content-Length: 14\r\n\r\n"
      "Unauthorized\r\n");
  const MockRead kSuccess(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=iso-8859-1\r\n"
      "Content-Length: 3\r\n\r\n"
      "Yes");

  MockWrite writes[] = {
    // First round
    kGet,
    // Second round
    kGetAuth,
    // Third round
    kGetAuth,
    // Fourth round
    kGetAuth,
    // Competing request
    kGet,
  };
  MockRead reads[] = {
    // First round
    kServerChallenge,
    // Second round
    kServerChallenge,
    // Third round
    kServerChallenge,
    // Fourth round
    kSuccess,
    // Competing response
    kSuccess,
  };
  StaticSocketDataProvider data_provider(reads, arraysize(reads),
                                         writes, arraysize(writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data_provider);

  const char* const kSocketGroup = "www.example.com:80";

  // First round of authentication.
  auth_handler->SetGenerateExpectation(false, OK);
  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_FALSE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(0, transport_pool->IdleSocketCountInGroup(kSocketGroup));

  // In between rounds, another request comes in for the same domain.
  // It should not be able to grab the TCP socket that trans has already
  // claimed.
  scoped_ptr<HttpTransaction> trans_compete(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  TestCompletionCallback callback_compete;
  rv = trans_compete->Start(
      &request, callback_compete.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // callback_compete.WaitForResult at this point would stall forever,
  // since the HttpNetworkTransaction does not release the request back to
  // the pool until after authentication completes.

  // Second round of authentication.
  auth_handler->SetGenerateExpectation(false, OK);
  rv = trans->RestartWithAuth(AuthCredentials(kFoo, kBar), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(0, transport_pool->IdleSocketCountInGroup(kSocketGroup));

  // Third round of authentication.
  auth_handler->SetGenerateExpectation(false, OK);
  rv = trans->RestartWithAuth(AuthCredentials(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(0, transport_pool->IdleSocketCountInGroup(kSocketGroup));

  // Fourth round of authentication, which completes successfully.
  auth_handler->SetGenerateExpectation(false, OK);
  rv = trans->RestartWithAuth(AuthCredentials(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(0, transport_pool->IdleSocketCountInGroup(kSocketGroup));

  // Read the body since the fourth round was successful. This will also
  // release the socket back to the pool.
  scoped_refptr<IOBufferWithSize> io_buf(new IOBufferWithSize(50));
  rv = trans->Read(io_buf.get(), io_buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(3, rv);
  rv = trans->Read(io_buf.get(), io_buf->size(), callback.callback());
  EXPECT_EQ(0, rv);
  // There are still 0 idle sockets, since the trans_compete transaction
  // will be handed it immediately after trans releases it to the group.
  EXPECT_EQ(0, transport_pool->IdleSocketCountInGroup(kSocketGroup));

  // The competing request can now finish. Wait for the headers and then
  // read the body.
  rv = callback_compete.WaitForResult();
  EXPECT_EQ(OK, rv);
  rv = trans_compete->Read(io_buf.get(), io_buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(3, rv);
  rv = trans_compete->Read(io_buf.get(), io_buf->size(), callback.callback());
  EXPECT_EQ(0, rv);

  // Finally, the socket is released to the group.
  EXPECT_EQ(1, transport_pool->IdleSocketCountInGroup(kSocketGroup));
}

// This tests the case that a request is issued via http instead of spdy after
// npn is negotiated.
TEST_P(HttpNetworkTransactionTest, NpnWithHttpOverSSL) {
  session_deps_.use_alternate_protocols = true;
  NextProtoVector next_protos;
  next_protos.push_back(kProtoHTTP11);
  session_deps_.next_protos = next_protos;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  std::string alternate_protocol_http_header =
      GetAlternateProtocolHttpHeader();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(alternate_protocol_http_header.c_str()),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  ssl.next_proto = "http/1.1";
  ssl.protocol_negotiated = kProtoHTTP11;

  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
}

TEST_P(HttpNetworkTransactionTest, SpdyPostNPNServerHangup) {
  // Simulate the SSL handshake completing with an NPN negotiation
  // followed by an immediate server closing of the socket.
  // Fix crash:  http://crbug.com/46369
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  MockRead spdy_reads[] = {
    MockRead(SYNCHRONOUS, 0, 0)   // Not async - return 0 immediately.
  };

  DelayedSocketData spdy_data(
      0,  // don't wait in this case, immediate hangup.
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  TestCompletionCallback callback;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback.WaitForResult());
}

// A subclass of HttpAuthHandlerMock that records the request URL when
// it gets it. This is needed since the auth handler may get destroyed
// before we get a chance to query it.
class UrlRecordingHttpAuthHandlerMock : public HttpAuthHandlerMock {
 public:
  explicit UrlRecordingHttpAuthHandlerMock(GURL* url) : url_(url) {}

  virtual ~UrlRecordingHttpAuthHandlerMock() {}

 protected:
  virtual int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                                    const HttpRequestInfo* request,
                                    const CompletionCallback& callback,
                                    std::string* auth_token) OVERRIDE {
    *url_ = request->url;
    return HttpAuthHandlerMock::GenerateAuthTokenImpl(
        credentials, request, callback, auth_token);
  }

 private:
  GURL* url_;
};

TEST_P(HttpNetworkTransactionTest, SpdyAlternateProtocolThroughProxy) {
  // This test ensures that the URL passed into the proxy is upgraded
  // to https when doing an Alternate Protocol upgrade.
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  CapturingNetLog net_log;
  session_deps_.net_log = &net_log;
  GURL request_url;
  {
    HttpAuthHandlerMock::Factory* auth_factory =
        new HttpAuthHandlerMock::Factory();
    UrlRecordingHttpAuthHandlerMock* auth_handler =
        new UrlRecordingHttpAuthHandlerMock(&request_url);
    auth_factory->AddMockHandler(auth_handler, HttpAuth::AUTH_PROXY);
    auth_factory->set_do_init_from_challenge(true);
    session_deps_.http_auth_handler_factory.reset(auth_factory);
  }

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com");
  request.load_flags = 0;

  // First round goes unauthenticated through the proxy.
  MockWrite data_writes_1[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "\r\n"),
  };
  MockRead data_reads_1[] = {
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead("HTTP/1.1 200 OK\r\n"
             "Alternate-Protocol: 443:npn-spdy/2\r\n"
             "Proxy-Connection: close\r\n"
             "\r\n"),
  };
  StaticSocketDataProvider data_1(data_reads_1, arraysize(data_reads_1),
                                  data_writes_1, arraysize(data_writes_1));

  // Second round tries to tunnel to www.google.com due to the
  // Alternate-Protocol announcement in the first round. It fails due
  // to a proxy authentication challenge.
  // After the failure, a tunnel is established to www.google.com using
  // Proxy-Authorization headers. There is then a SPDY request round.
  //
  // NOTE: Despite the "Proxy-Connection: Close", these are done on the
  // same MockTCPClientSocket since the underlying HttpNetworkClientSocket
  // does a Disconnect and Connect on the same socket, rather than trying
  // to obtain a new one.
  //
  // NOTE: Originally, the proxy response to the second CONNECT request
  // simply returned another 407 so the unit test could skip the SSL connection
  // establishment and SPDY framing issues. Alas, the
  // retry-http-when-alternate-protocol fails logic kicks in, which was more
  // complicated to set up expectations for than the SPDY session.

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));

  MockWrite data_writes_2[] = {
    // First connection attempt without Proxy-Authorization.
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "\r\n"),

    // Second connection attempt with Proxy-Authorization.
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: auth_token\r\n"
              "\r\n"),

    // SPDY request
    CreateMockWrite(*req),
  };
  const char kRejectConnectResponse[] = ("HTTP/1.1 407 Unauthorized\r\n"
                                         "Proxy-Authenticate: Mock\r\n"
                                         "Proxy-Connection: close\r\n"
                                         "\r\n");
  const char kAcceptConnectResponse[] = "HTTP/1.1 200 Connected\r\n\r\n";
  MockRead data_reads_2[] = {
    // First connection attempt fails
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ, 1),
    MockRead(ASYNC, kRejectConnectResponse,
             arraysize(kRejectConnectResponse) - 1, 1),

    // Second connection attempt passes
    MockRead(ASYNC, kAcceptConnectResponse,
             arraysize(kAcceptConnectResponse) -1, 4),

    // SPDY response
    CreateMockRead(*resp.get(), 6),
    CreateMockRead(*data.get(), 6),
    MockRead(ASYNC, 0, 0, 6),
  };
  OrderedSocketData data_2(
      data_reads_2, arraysize(data_reads_2),
      data_writes_2, arraysize(data_writes_2));

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_non_alternate_protocol_socket(
      NULL, 0, NULL, 0);
  hanging_non_alternate_protocol_socket.set_connect_data(
      never_finishing_connect);

  session_deps_.socket_factory->AddSocketDataProvider(&data_1);
  session_deps_.socket_factory->AddSocketDataProvider(&data_2);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(
      &hanging_non_alternate_protocol_socket);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // First round should work and provide the Alternate-Protocol state.
  TestCompletionCallback callback_1;
  scoped_ptr<HttpTransaction> trans_1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  int rv = trans_1->Start(&request, callback_1.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback_1.WaitForResult());

  // Second round should attempt a tunnel connect and get an auth challenge.
  TestCompletionCallback callback_2;
  scoped_ptr<HttpTransaction> trans_2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  rv = trans_2->Start(&request, callback_2.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback_2.WaitForResult());
  const HttpResponseInfo* response = trans_2->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_FALSE(response->auth_challenge.get() == NULL);

  // Restart with auth. Tunnel should work and response received.
  TestCompletionCallback callback_3;
  rv = trans_2->RestartWithAuth(
      AuthCredentials(kFoo, kBar), callback_3.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback_3.WaitForResult());

  // After all that work, these two lines (or actually, just the scheme) are
  // what this test is all about. Make sure it happens correctly.
  EXPECT_EQ("https", request_url.scheme());
  EXPECT_EQ("www.google.com", request_url.host());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans_2->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);
}

// Test that if we cancel the transaction as the connection is completing, that
// everything tears down correctly.
TEST_P(HttpNetworkTransactionTest, SimpleCancel) {
  // Setup everything about the connection to complete synchronously, so that
  // after calling HttpNetworkTransaction::Start, the only thing we're waiting
  // for is the callback from the HttpStreamRequest.
  // Then cancel the transaction.
  // Verify that we don't crash.
  MockConnect mock_connect(SYNCHRONOUS, OK);
  MockRead data_reads[] = {
    MockRead(SYNCHRONOUS, "HTTP/1.0 200 OK\r\n\r\n"),
    MockRead(SYNCHRONOUS, "hello world"),
    MockRead(SYNCHRONOUS, OK),
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  session_deps_.host_resolver->set_synchronous_mode(true);
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  CapturingBoundNetLog log;
  int rv = trans->Start(&request, callback.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  trans.reset();  // Cancel the transaction here.

  base::MessageLoop::current()->RunUntilIdle();
}

// Test that if a transaction is cancelled after receiving the headers, the
// stream is drained properly and added back to the socket pool.  The main
// purpose of this test is to make sure that an HttpStreamParser can be read
// from after the HttpNetworkTransaction and the objects it owns have been
// deleted.
// See http://crbug.com/368418
TEST_P(HttpNetworkTransactionTest, CancelAfterHeaders) {
  MockRead data_reads[] = {
    MockRead(ASYNC, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, "Content-Length: 2\r\n"),
    MockRead(ASYNC, "Connection: Keep-Alive\r\n\r\n"),
    MockRead(ASYNC, "1"),
    // 2 async reads are necessary to trigger a ReadResponseBody call after the
    // HttpNetworkTransaction has been deleted.
    MockRead(ASYNC, "2"),
    MockRead(SYNCHRONOUS, ERR_IO_PENDING),  // Should never read this.
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  {
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session);
    TestCompletionCallback callback;

    int rv = trans.Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    callback.WaitForResult();

    const HttpResponseInfo* response = trans.GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    EXPECT_TRUE(response->headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    // The transaction and HttpRequestInfo are deleted.
  }

  // Let the HttpResponseBodyDrainer drain the socket.
  base::MessageLoop::current()->RunUntilIdle();

  // Socket should now be idle, waiting to be reused.
  EXPECT_EQ(1, GetIdleSocketCountInTransportSocketPool(session));
}

// Test a basic GET request through a proxy.
TEST_P(HttpNetworkTransactionTest, ProxyGet) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");

  MockWrite data_writes1[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(100, response->headers->GetContentLength());
  EXPECT_TRUE(response->was_fetched_via_proxy);
  EXPECT_TRUE(
      response->proxy_server.Equals(HostPortPair::FromString("myproxy:70")));
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);
}

// Test a basic HTTPS GET request through a proxy.
TEST_P(HttpNetworkTransactionTest, ProxyTunnelGet) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLog::PHASE_NONE);
  ExpectLogContainsSomewhere(
      entries, pos,
      NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLog::PHASE_NONE);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(200, response->headers->response_code());
  EXPECT_EQ(100, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());
  EXPECT_TRUE(response->was_fetched_via_proxy);
  EXPECT_TRUE(
      response->proxy_server.Equals(HostPortPair::FromString("myproxy:70")));

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(trans->GetLoadTimingInfo(&load_timing_info));
  TestLoadTimingNotReusedWithPac(load_timing_info,
                                 CONNECT_TIMING_HAS_SSL_TIMES);
}

// Test a basic HTTPS GET request through a proxy, but the server hangs up
// while establishing the tunnel.
TEST_P(HttpNetworkTransactionTest, ProxyTunnelGetHangup) {
  session_deps_.proxy_service.reset(ProxyService::CreateFixed("myproxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
    MockRead(ASYNC, 0, 0),  // EOF
  };

  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback1;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request, callback1.callback(), log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(ERR_EMPTY_RESPONSE, rv);
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLog::PHASE_NONE);
  ExpectLogContainsSomewhere(
      entries, pos,
      NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLog::PHASE_NONE);
}

// Test for crbug.com/55424.
TEST_P(HttpNetworkTransactionTest, PreconnectWithExistingSpdySession) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.google.com", false, 1, LOWEST));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> data(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*data),
    MockRead(ASYNC, 0, 0),
  };

  DelayedSocketData spdy_data(
      1,  // wait for one write to finish before reading.
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Set up an initial SpdySession in the pool to reuse.
  HostPortPair host_port_pair("www.google.com", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> spdy_session =
      CreateInsecureSpdySession(session, key, BoundNetLog());

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // This is the important line that marks this as a preconnect.
  request.motivation = HttpRequestInfo::PRECONNECT_MOTIVATED;

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  TestCompletionCallback callback;
  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
}

// Given a net error, cause that error to be returned from the first Write()
// call and verify that the HttpTransaction fails with that error.
void HttpNetworkTransactionTest::CheckErrorIsPassedBack(
    int error, IoMode mode) {
  net::HttpRequestInfo request_info;
  request_info.url = GURL("https://www.example.com/");
  request_info.method = "GET";
  request_info.load_flags = net::LOAD_NORMAL;

  SSLSocketDataProvider ssl_data(mode, OK);
  net::MockWrite data_writes[] = {
    net::MockWrite(mode, error),
  };
  net::StaticSocketDataProvider data(NULL, 0,
                                     data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  TestCompletionCallback callback;
  int rv = trans->Start(&request_info, callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(error, rv);
}

TEST_P(HttpNetworkTransactionTest, SSLWriteCertError) {
  // Just check a grab bag of cert errors.
  static const int kErrors[] = {
    ERR_CERT_COMMON_NAME_INVALID,
    ERR_CERT_AUTHORITY_INVALID,
    ERR_CERT_DATE_INVALID,
  };
  for (size_t i = 0; i < arraysize(kErrors); i++) {
    CheckErrorIsPassedBack(kErrors[i], ASYNC);
    CheckErrorIsPassedBack(kErrors[i], SYNCHRONOUS);
  }
}

// Ensure that a client certificate is removed from the SSL client auth
// cache when:
//  1) No proxy is involved.
//  2) TLS False Start is disabled.
//  3) The initial TLS handshake requests a client certificate.
//  4) The client supplies an invalid/unacceptable certificate.
TEST_P(HttpNetworkTransactionTest,
       ClientAuthCertCache_Direct_NoFalseStart) {
  net::HttpRequestInfo request_info;
  request_info.url = GURL("https://www.example.com/");
  request_info.method = "GET";
  request_info.load_flags = net::LOAD_NORMAL;

  scoped_refptr<SSLCertRequestInfo> cert_request(new SSLCertRequestInfo());
  cert_request->host_and_port = HostPortPair("www.example.com", 443);

  // [ssl_]data1 contains the data for the first SSL handshake. When a
  // CertificateRequest is received for the first time, the handshake will
  // be aborted to allow the caller to provide a certificate.
  SSLSocketDataProvider ssl_data1(ASYNC, net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl_data1.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data1);
  net::StaticSocketDataProvider data1(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  // [ssl_]data2 contains the data for the second SSL handshake. When TLS
  // False Start is not being used, the result of the SSL handshake will be
  // returned as part of the SSLClientSocket::Connect() call. This test
  // matches the result of a server sending a handshake_failure alert,
  // rather than a Finished message, because it requires a client
  // certificate and none was supplied.
  SSLSocketDataProvider ssl_data2(ASYNC, net::ERR_SSL_PROTOCOL_ERROR);
  ssl_data2.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data2);
  net::StaticSocketDataProvider data2(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  // [ssl_]data3 contains the data for the third SSL handshake. When a
  // connection to a server fails during an SSL handshake,
  // HttpNetworkTransaction will attempt to fallback to TLSv1 if the previous
  // connection was attempted with TLSv1.1. This is transparent to the caller
  // of the HttpNetworkTransaction. Because this test failure is due to
  // requiring a client certificate, this fallback handshake should also
  // fail.
  SSLSocketDataProvider ssl_data3(ASYNC, net::ERR_SSL_PROTOCOL_ERROR);
  ssl_data3.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data3);
  net::StaticSocketDataProvider data3(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);

  // [ssl_]data4 contains the data for the fourth SSL handshake. When a
  // connection to a server fails during an SSL handshake,
  // HttpNetworkTransaction will attempt to fallback to SSLv3 if the previous
  // connection was attempted with TLSv1. This is transparent to the caller
  // of the HttpNetworkTransaction. Because this test failure is due to
  // requiring a client certificate, this fallback handshake should also
  // fail.
  SSLSocketDataProvider ssl_data4(ASYNC, net::ERR_SSL_PROTOCOL_ERROR);
  ssl_data4.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data4);
  net::StaticSocketDataProvider data4(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data4);

  // Need one more if TLSv1.2 is enabled.
  SSLSocketDataProvider ssl_data5(ASYNC, net::ERR_SSL_PROTOCOL_ERROR);
  ssl_data5.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data5);
  net::StaticSocketDataProvider data5(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data5);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Begin the SSL handshake with the peer. This consumes ssl_data1.
  TestCompletionCallback callback;
  int rv = trans->Start(&request_info, callback.callback(), net::BoundNetLog());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // Complete the SSL handshake, which should abort due to requiring a
  // client certificate.
  rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);

  // Indicate that no certificate should be supplied. From the perspective
  // of SSLClientCertCache, NULL is just as meaningful as a real
  // certificate, so this is the same as supply a
  // legitimate-but-unacceptable certificate.
  rv = trans->RestartWithCertificate(NULL, callback.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // Ensure the certificate was added to the client auth cache before
  // allowing the connection to continue restarting.
  scoped_refptr<X509Certificate> client_cert;
  ASSERT_TRUE(session->ssl_client_auth_cache()->Lookup(
      HostPortPair("www.example.com", 443), &client_cert));
  ASSERT_EQ(NULL, client_cert.get());

  // Restart the handshake. This will consume ssl_data2, which fails, and
  // then consume ssl_data3 and ssl_data4, both of which should also fail.
  // The result code is checked against what ssl_data4 should return.
  rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_SSL_PROTOCOL_ERROR, rv);

  // Ensure that the client certificate is removed from the cache on a
  // handshake failure.
  ASSERT_FALSE(session->ssl_client_auth_cache()->Lookup(
      HostPortPair("www.example.com", 443), &client_cert));
}

// Ensure that a client certificate is removed from the SSL client auth
// cache when:
//  1) No proxy is involved.
//  2) TLS False Start is enabled.
//  3) The initial TLS handshake requests a client certificate.
//  4) The client supplies an invalid/unacceptable certificate.
TEST_P(HttpNetworkTransactionTest,
       ClientAuthCertCache_Direct_FalseStart) {
  net::HttpRequestInfo request_info;
  request_info.url = GURL("https://www.example.com/");
  request_info.method = "GET";
  request_info.load_flags = net::LOAD_NORMAL;

  scoped_refptr<SSLCertRequestInfo> cert_request(new SSLCertRequestInfo());
  cert_request->host_and_port = HostPortPair("www.example.com", 443);

  // When TLS False Start is used, SSLClientSocket::Connect() calls will
  // return successfully after reading up to the peer's Certificate message.
  // This is to allow the caller to call SSLClientSocket::Write(), which can
  // enqueue application data to be sent in the same packet as the
  // ChangeCipherSpec and Finished messages.
  // The actual handshake will be finished when SSLClientSocket::Read() is
  // called, which expects to process the peer's ChangeCipherSpec and
  // Finished messages. If there was an error negotiating with the peer,
  // such as due to the peer requiring a client certificate when none was
  // supplied, the alert sent by the peer won't be processed until Read() is
  // called.

  // Like the non-False Start case, when a client certificate is requested by
  // the peer, the handshake is aborted during the Connect() call.
  // [ssl_]data1 represents the initial SSL handshake with the peer.
  SSLSocketDataProvider ssl_data1(ASYNC, net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl_data1.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data1);
  net::StaticSocketDataProvider data1(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  // When a client certificate is supplied, Connect() will not be aborted
  // when the peer requests the certificate. Instead, the handshake will
  // artificially succeed, allowing the caller to write the HTTP request to
  // the socket. The handshake messages are not processed until Read() is
  // called, which then detects that the handshake was aborted, due to the
  // peer sending a handshake_failure because it requires a client
  // certificate.
  SSLSocketDataProvider ssl_data2(ASYNC, net::OK);
  ssl_data2.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data2);
  net::MockRead data2_reads[] = {
    net::MockRead(ASYNC /* async */, net::ERR_SSL_PROTOCOL_ERROR),
  };
  net::StaticSocketDataProvider data2(
      data2_reads, arraysize(data2_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  // As described in ClientAuthCertCache_Direct_NoFalseStart, [ssl_]data3 is
  // the data for the SSL handshake once the TLSv1.1 connection falls back to
  // TLSv1. It has the same behaviour as [ssl_]data2.
  SSLSocketDataProvider ssl_data3(ASYNC, net::OK);
  ssl_data3.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data3);
  net::StaticSocketDataProvider data3(
      data2_reads, arraysize(data2_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);

  // [ssl_]data4 is the data for the SSL handshake once the TLSv1 connection
  // falls back to SSLv3. It has the same behaviour as [ssl_]data2.
  SSLSocketDataProvider ssl_data4(ASYNC, net::OK);
  ssl_data4.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data4);
  net::StaticSocketDataProvider data4(
      data2_reads, arraysize(data2_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data4);

  // Need one more if TLSv1.2 is enabled.
  SSLSocketDataProvider ssl_data5(ASYNC, net::OK);
  ssl_data5.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data5);
  net::StaticSocketDataProvider data5(
      data2_reads, arraysize(data2_reads), NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data5);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  // Begin the initial SSL handshake.
  TestCompletionCallback callback;
  int rv = trans->Start(&request_info, callback.callback(), net::BoundNetLog());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // Complete the SSL handshake, which should abort due to requiring a
  // client certificate.
  rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);

  // Indicate that no certificate should be supplied. From the perspective
  // of SSLClientCertCache, NULL is just as meaningful as a real
  // certificate, so this is the same as supply a
  // legitimate-but-unacceptable certificate.
  rv = trans->RestartWithCertificate(NULL, callback.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // Ensure the certificate was added to the client auth cache before
  // allowing the connection to continue restarting.
  scoped_refptr<X509Certificate> client_cert;
  ASSERT_TRUE(session->ssl_client_auth_cache()->Lookup(
      HostPortPair("www.example.com", 443), &client_cert));
  ASSERT_EQ(NULL, client_cert.get());

  // Restart the handshake. This will consume ssl_data2, which fails, and
  // then consume ssl_data3 and ssl_data4, both of which should also fail.
  // The result code is checked against what ssl_data4 should return.
  rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_SSL_PROTOCOL_ERROR, rv);

  // Ensure that the client certificate is removed from the cache on a
  // handshake failure.
  ASSERT_FALSE(session->ssl_client_auth_cache()->Lookup(
      HostPortPair("www.example.com", 443), &client_cert));
}

// Ensure that a client certificate is removed from the SSL client auth
// cache when:
//  1) An HTTPS proxy is involved.
//  3) The HTTPS proxy requests a client certificate.
//  4) The client supplies an invalid/unacceptable certificate for the
//     proxy.
// The test is repeated twice, first for connecting to an HTTPS endpoint,
// then for connecting to an HTTP endpoint.
TEST_P(HttpNetworkTransactionTest, ClientAuthCertCache_Proxy_Fail) {
  session_deps_.proxy_service.reset(
      ProxyService::CreateFixed("https://proxy:70"));
  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();

  scoped_refptr<SSLCertRequestInfo> cert_request(new SSLCertRequestInfo());
  cert_request->host_and_port = HostPortPair("proxy", 70);

  // See ClientAuthCertCache_Direct_NoFalseStart for the explanation of
  // [ssl_]data[1-3]. Rather than represending the endpoint
  // (www.example.com:443), they represent failures with the HTTPS proxy
  // (proxy:70).
  SSLSocketDataProvider ssl_data1(ASYNC, net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl_data1.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data1);
  net::StaticSocketDataProvider data1(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);

  SSLSocketDataProvider ssl_data2(ASYNC, net::ERR_SSL_PROTOCOL_ERROR);
  ssl_data2.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data2);
  net::StaticSocketDataProvider data2(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  // TODO(wtc): find out why this unit test doesn't need [ssl_]data3.
#if 0
  SSLSocketDataProvider ssl_data3(ASYNC, net::ERR_SSL_PROTOCOL_ERROR);
  ssl_data3.cert_request_info = cert_request.get();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data3);
  net::StaticSocketDataProvider data3(NULL, 0, NULL, 0);
  session_deps_.socket_factory->AddSocketDataProvider(&data3);
#endif

  net::HttpRequestInfo requests[2];
  requests[0].url = GURL("https://www.example.com/");
  requests[0].method = "GET";
  requests[0].load_flags = net::LOAD_NORMAL;

  requests[1].url = GURL("http://www.example.com/");
  requests[1].method = "GET";
  requests[1].load_flags = net::LOAD_NORMAL;

  for (size_t i = 0; i < arraysize(requests); ++i) {
    session_deps_.socket_factory->ResetNextMockIndexes();
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

    // Begin the SSL handshake with the proxy.
    TestCompletionCallback callback;
    int rv = trans->Start(
        &requests[i], callback.callback(), net::BoundNetLog());
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    // Complete the SSL handshake, which should abort due to requiring a
    // client certificate.
    rv = callback.WaitForResult();
    ASSERT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);

    // Indicate that no certificate should be supplied. From the perspective
    // of SSLClientCertCache, NULL is just as meaningful as a real
    // certificate, so this is the same as supply a
    // legitimate-but-unacceptable certificate.
    rv = trans->RestartWithCertificate(NULL, callback.callback());
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    // Ensure the certificate was added to the client auth cache before
    // allowing the connection to continue restarting.
    scoped_refptr<X509Certificate> client_cert;
    ASSERT_TRUE(session->ssl_client_auth_cache()->Lookup(
        HostPortPair("proxy", 70), &client_cert));
    ASSERT_EQ(NULL, client_cert.get());
    // Ensure the certificate was NOT cached for the endpoint. This only
    // applies to HTTPS requests, but is fine to check for HTTP requests.
    ASSERT_FALSE(session->ssl_client_auth_cache()->Lookup(
        HostPortPair("www.example.com", 443), &client_cert));

    // Restart the handshake. This will consume ssl_data2, which fails, and
    // then consume ssl_data3, which should also fail. The result code is
    // checked against what ssl_data3 should return.
    rv = callback.WaitForResult();
    ASSERT_EQ(net::ERR_PROXY_CONNECTION_FAILED, rv);

    // Now that the new handshake has failed, ensure that the client
    // certificate was removed from the client auth cache.
    ASSERT_FALSE(session->ssl_client_auth_cache()->Lookup(
        HostPortPair("proxy", 70), &client_cert));
    ASSERT_FALSE(session->ssl_client_auth_cache()->Lookup(
        HostPortPair("www.example.com", 443), &client_cert));
  }
}

// Unlike TEST/TEST_F, which are macros that expand to further macros,
// TEST_P is a macro that expands directly to code that stringizes the
// arguments. As a result, macros passed as parameters (such as prefix
// or test_case_name) will not be expanded by the preprocessor. To
// work around this, indirect the macro for TEST_P, so that the
// pre-processor will expand macros such as MAYBE_test_name before
// instantiating the test.
#define WRAPPED_TEST_P(test_case_name, test_name) \
  TEST_P(test_case_name, test_name)

// Times out on Win7 dbg(2) bot. http://crbug.com/124776
#if defined(OS_WIN)
#define MAYBE_UseIPConnectionPooling DISABLED_UseIPConnectionPooling
#else
#define MAYBE_UseIPConnectionPooling UseIPConnectionPooling
#endif
WRAPPED_TEST_P(HttpNetworkTransactionTest, MAYBE_UseIPConnectionPooling) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  // Set up a special HttpNetworkSession with a MockCachingHostResolver.
  session_deps_.host_resolver.reset(new MockCachingHostResolver());
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  SpdySessionPoolPeer pool_peer(session->spdy_session_pool());
  pool_peer.DisableDomainAuthenticationVerification();

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> host1_req(
      spdy_util_.ConstructSpdyGet("https://www.google.com", false, 1, LOWEST));
  scoped_ptr<SpdyFrame> host2_req(
      spdy_util_.ConstructSpdyGet("https://www.gmail.com", false, 3, LOWEST));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*host1_req, 1),
    CreateMockWrite(*host2_req, 4),
  };
  scoped_ptr<SpdyFrame> host1_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> host1_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> host2_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> host2_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*host1_resp, 2),
    CreateMockRead(*host1_resp_body, 3),
    CreateMockRead(*host2_resp, 5),
    CreateMockRead(*host2_resp_body, 6),
    MockRead(ASYNC, 0, 7),
  };

  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &ip));
  IPEndPoint peer_addr = IPEndPoint(ip, 443);
  MockConnect connect(ASYNC, OK, peer_addr);
  OrderedSocketData spdy_data(
      connect,
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  TestCompletionCallback callback;
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/");
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session.get());

  int rv = trans1.Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(&trans1, &response_data));
  EXPECT_EQ("hello!", response_data);

  // Preload www.gmail.com into HostCache.
  HostPortPair host_port("www.gmail.com", 443);
  HostResolver::RequestInfo resolve_info(host_port);
  AddressList ignored;
  rv = session_deps_.host_resolver->Resolve(resolve_info,
                                            DEFAULT_PRIORITY,
                                            &ignored,
                                            callback.callback(),
                                            NULL,
                                            BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.gmail.com/");
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session.get());

  rv = trans2.Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans2.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(&trans2, &response_data));
  EXPECT_EQ("hello!", response_data);
}
#undef MAYBE_UseIPConnectionPooling

TEST_P(HttpNetworkTransactionTest, UseIPConnectionPoolingAfterResolution) {
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  // Set up a special HttpNetworkSession with a MockCachingHostResolver.
  session_deps_.host_resolver.reset(new MockCachingHostResolver());
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  SpdySessionPoolPeer pool_peer(session->spdy_session_pool());
  pool_peer.DisableDomainAuthenticationVerification();

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> host1_req(
      spdy_util_.ConstructSpdyGet("https://www.google.com", false, 1, LOWEST));
  scoped_ptr<SpdyFrame> host2_req(
      spdy_util_.ConstructSpdyGet("https://www.gmail.com", false, 3, LOWEST));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*host1_req, 1),
    CreateMockWrite(*host2_req, 4),
  };
  scoped_ptr<SpdyFrame> host1_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> host1_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> host2_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> host2_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*host1_resp, 2),
    CreateMockRead(*host1_resp_body, 3),
    CreateMockRead(*host2_resp, 5),
    CreateMockRead(*host2_resp_body, 6),
    MockRead(ASYNC, 0, 7),
  };

  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &ip));
  IPEndPoint peer_addr = IPEndPoint(ip, 443);
  MockConnect connect(ASYNC, OK, peer_addr);
  OrderedSocketData spdy_data(
      connect,
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  TestCompletionCallback callback;
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/");
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session.get());

  int rv = trans1.Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(&trans1, &response_data));
  EXPECT_EQ("hello!", response_data);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.gmail.com/");
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session.get());

  rv = trans2.Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans2.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(&trans2, &response_data));
  EXPECT_EQ("hello!", response_data);
}

class OneTimeCachingHostResolver : public net::HostResolver {
 public:
  explicit OneTimeCachingHostResolver(const HostPortPair& host_port)
      : host_port_(host_port) {}
  virtual ~OneTimeCachingHostResolver() {}

  RuleBasedHostResolverProc* rules() { return host_resolver_.rules(); }

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) OVERRIDE {
    return host_resolver_.Resolve(
        info, priority, addresses, callback, out_req, net_log);
  }

  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& net_log) OVERRIDE {
    int rv = host_resolver_.ResolveFromCache(info, addresses, net_log);
    if (rv == OK && info.host_port_pair().Equals(host_port_))
      host_resolver_.GetHostCache()->clear();
    return rv;
  }

  virtual void CancelRequest(RequestHandle req) OVERRIDE {
    host_resolver_.CancelRequest(req);
  }

  MockCachingHostResolver* GetMockHostResolver() {
    return &host_resolver_;
  }

 private:
  MockCachingHostResolver host_resolver_;
  const HostPortPair host_port_;
};

// Times out on Win7 dbg(2) bot. http://crbug.com/124776
#if defined(OS_WIN)
#define MAYBE_UseIPConnectionPoolingWithHostCacheExpiration \
    DISABLED_UseIPConnectionPoolingWithHostCacheExpiration
#else
#define MAYBE_UseIPConnectionPoolingWithHostCacheExpiration \
    UseIPConnectionPoolingWithHostCacheExpiration
#endif
WRAPPED_TEST_P(HttpNetworkTransactionTest,
               MAYBE_UseIPConnectionPoolingWithHostCacheExpiration) {
// Times out on Win7 dbg(2) bot. http://crbug.com/124776 . (MAYBE_
// prefix doesn't work with parametrized tests).
#if defined(OS_WIN)
  return;
#else
  session_deps_.use_alternate_protocols = true;
  session_deps_.next_protos = SpdyNextProtos();

  // Set up a special HttpNetworkSession with a OneTimeCachingHostResolver.
  OneTimeCachingHostResolver host_resolver(HostPortPair("www.gmail.com", 443));
  HttpNetworkSession::Params params =
      SpdySessionDependencies::CreateSessionParams(&session_deps_);
  params.host_resolver = &host_resolver;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  SpdySessionPoolPeer pool_peer(session->spdy_session_pool());
  pool_peer.DisableDomainAuthenticationVerification();

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_ptr<SpdyFrame> host1_req(
      spdy_util_.ConstructSpdyGet("https://www.google.com", false, 1, LOWEST));
  scoped_ptr<SpdyFrame> host2_req(
      spdy_util_.ConstructSpdyGet("https://www.gmail.com", false, 3, LOWEST));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*host1_req, 1),
    CreateMockWrite(*host2_req, 4),
  };
  scoped_ptr<SpdyFrame> host1_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> host1_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> host2_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> host2_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*host1_resp, 2),
    CreateMockRead(*host1_resp_body, 3),
    CreateMockRead(*host2_resp, 5),
    CreateMockRead(*host2_resp_body, 6),
    MockRead(ASYNC, 0, 7),
  };

  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &ip));
  IPEndPoint peer_addr = IPEndPoint(ip, 443);
  MockConnect connect(ASYNC, OK, peer_addr);
  OrderedSocketData spdy_data(
      connect,
      spdy_reads, arraysize(spdy_reads),
      spdy_writes, arraysize(spdy_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&spdy_data);

  TestCompletionCallback callback;
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.google.com/");
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, session.get());

  int rv = trans1.Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans1.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(&trans1, &response_data));
  EXPECT_EQ("hello!", response_data);

  // Preload cache entries into HostCache.
  HostResolver::RequestInfo resolve_info(HostPortPair("www.gmail.com", 443));
  AddressList ignored;
  rv = host_resolver.Resolve(resolve_info,
                             DEFAULT_PRIORITY,
                             &ignored,
                             callback.callback(),
                             NULL,
                             BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.gmail.com/");
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session.get());

  rv = trans2.Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans2.GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(&trans2, &response_data));
  EXPECT_EQ("hello!", response_data);
#endif
}
#undef MAYBE_UseIPConnectionPoolingWithHostCacheExpiration

TEST_P(HttpNetworkTransactionTest, DoNotUseSpdySessionForHttp) {
  const std::string https_url = "https://www.google.com/";
  const std::string http_url = "http://www.google.com:443/";

  // SPDY GET for HTTPS URL
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(https_url.c_str(), false, 1, LOWEST));

  MockWrite writes1[] = {
    CreateMockWrite(*req1, 0),
  };

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads1[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*body1, 2),
    MockRead(ASYNC, ERR_IO_PENDING, 3)
  };

  DelayedSocketData data1(
      1, reads1, arraysize(reads1),
      writes1, arraysize(writes1));
  MockConnect connect_data1(ASYNC, OK);
  data1.set_connect_data(connect_data1);

  // HTTP GET for the HTTP URL
  MockWrite writes2[] = {
    MockWrite(ASYNC, 4,
              "GET / HTTP/1.1\r\n"
              "Host: www.google.com:443\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead reads2[] = {
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead(ASYNC, 6, "hello"),
    MockRead(ASYNC, 7, OK),
  };

  DelayedSocketData data2(
      1, reads2, arraysize(reads2),
      writes2, arraysize(writes2));

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Start the first transaction to set up the SpdySession
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL(https_url);
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(LOWEST, session.get());
  TestCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            trans1.Start(&request1, callback1.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_TRUE(trans1.GetResponseInfo()->was_fetched_via_spdy);

  // Now, start the HTTP request
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(http_url);
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(MEDIUM, session.get());
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            trans2.Start(&request2, callback2.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(trans2.GetResponseInfo()->was_fetched_via_spdy);
}

TEST_P(HttpNetworkTransactionTest, DoNotUseSpdySessionForHttpOverTunnel) {
  const std::string https_url = "https://www.google.com/";
  const std::string http_url = "http://www.google.com:443/";

  // SPDY GET for HTTPS URL (through CONNECT tunnel)
  scoped_ptr<SpdyFrame> connect(spdy_util_.ConstructSpdyConnect(NULL, 0, 1,
                                                                LOWEST));
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(https_url.c_str(), false, 1, LOWEST));
  scoped_ptr<SpdyFrame> wrapped_req1(
      spdy_util_.ConstructWrappedSpdyFrame(req1, 1));

  // SPDY GET for HTTP URL (through the proxy, but not the tunnel).
  SpdySynStreamIR req2_ir(3);
  spdy_util_.SetPriority(MEDIUM, &req2_ir);
  req2_ir.set_fin(true);
  req2_ir.SetHeader(spdy_util_.GetMethodKey(), "GET");
  req2_ir.SetHeader(spdy_util_.GetPathKey(),
                    spdy_util_.is_spdy2() ? http_url.c_str() : "/");
  req2_ir.SetHeader(spdy_util_.GetHostKey(), "www.google.com:443");
  req2_ir.SetHeader(spdy_util_.GetSchemeKey(), "http");
  spdy_util_.MaybeAddVersionHeader(&req2_ir);
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.CreateFramer(false)->SerializeFrame(req2_ir));

  MockWrite writes1[] = {
    CreateMockWrite(*connect, 0),
    CreateMockWrite(*wrapped_req1, 2),
    CreateMockWrite(*req2, 5),
  };

  scoped_ptr<SpdyFrame> conn_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> wrapped_resp1(
      spdy_util_.ConstructWrappedSpdyFrame(resp1, 1));
  scoped_ptr<SpdyFrame> wrapped_body1(
      spdy_util_.ConstructWrappedSpdyFrame(body1, 1));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead reads1[] = {
    CreateMockRead(*conn_resp, 1),
    CreateMockRead(*wrapped_resp1, 3),
    CreateMockRead(*wrapped_body1, 4),
    CreateMockRead(*resp2, 6),
    CreateMockRead(*body2, 7),
    MockRead(ASYNC, ERR_IO_PENDING, 8)
  };

  DeterministicSocketData data1(reads1, arraysize(reads1),
                                writes1, arraysize(writes1));
  MockConnect connect_data1(ASYNC, OK);
  data1.set_connect_data(connect_data1);

  session_deps_.proxy_service.reset(
      ProxyService::CreateFixedFromPacResult("HTTPS proxy:70"));
  CapturingNetLog log;
  session_deps_.net_log = &log;
  SSLSocketDataProvider ssl1(ASYNC, OK);  // to the proxy
  ssl1.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl1);
  SSLSocketDataProvider ssl2(ASYNC, OK);  // to the server
  ssl2.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl2);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data1);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_));

  // Start the first transaction to set up the SpdySession
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL(https_url);
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(LOWEST, session.get());
  TestCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            trans1.Start(&request1, callback1.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();
  data1.RunFor(4);

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_TRUE(trans1.GetResponseInfo()->was_fetched_via_spdy);

  LoadTimingInfo load_timing_info1;
  EXPECT_TRUE(trans1.GetLoadTimingInfo(&load_timing_info1));
  TestLoadTimingNotReusedWithPac(load_timing_info1,
                                 CONNECT_TIMING_HAS_SSL_TIMES);

  // Now, start the HTTP request
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(http_url);
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(MEDIUM, session.get());
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            trans2.Start(&request2, callback2.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();
  data1.RunFor(3);

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_TRUE(trans2.GetResponseInfo()->was_fetched_via_spdy);

  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(trans2.GetLoadTimingInfo(&load_timing_info2));
  // The established SPDY sessions is considered reused by the HTTP request.
  TestLoadTimingReusedWithPac(load_timing_info2);
  // HTTP requests over a SPDY session should have a different connection
  // socket_log_id than requests over a tunnel.
  EXPECT_NE(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);
}

TEST_P(HttpNetworkTransactionTest, UseSpdySessionForHttpWhenForced) {
  session_deps_.force_spdy_always = true;
  const std::string https_url = "https://www.google.com/";
  const std::string http_url = "http://www.google.com:443/";

  // SPDY GET for HTTPS URL
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(https_url.c_str(), false, 1, LOWEST));
  // SPDY GET for the HTTP URL
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(http_url.c_str(), false, 3, MEDIUM));

  MockWrite writes[] = {
    CreateMockWrite(*req1, 1),
    CreateMockWrite(*req2, 4),
  };

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead reads[] = {
    CreateMockRead(*resp1, 2),
    CreateMockRead(*body1, 3),
    CreateMockRead(*resp2, 5),
    CreateMockRead(*body2, 6),
    MockRead(ASYNC, ERR_IO_PENDING, 7)
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));

  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Start the first transaction to set up the SpdySession
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL(https_url);
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(LOWEST, session.get());
  TestCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            trans1.Start(&request1, callback1.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_TRUE(trans1.GetResponseInfo()->was_fetched_via_spdy);

  // Now, start the HTTP request
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(http_url);
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(MEDIUM, session.get());
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            trans2.Start(&request2, callback2.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_TRUE(trans2.GetResponseInfo()->was_fetched_via_spdy);
}

// Test that in the case where we have a SPDY session to a SPDY proxy
// that we do not pool other origins that resolve to the same IP when
// the certificate does not match the new origin.
// http://crbug.com/134690
TEST_P(HttpNetworkTransactionTest, DoNotUseSpdySessionIfCertDoesNotMatch) {
  const std::string url1 = "http://www.google.com/";
  const std::string url2 = "https://mail.google.com/";
  const std::string ip_addr = "1.2.3.4";

  // SPDY GET for HTTP URL (through SPDY proxy)
  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlockForProxy("http://www.google.com/"));
  scoped_ptr<SpdyFrame> req1(spdy_util_.ConstructSpdyControlFrame(
      headers.Pass(), false, 1, LOWEST, SYN_STREAM, CONTROL_FLAG_FIN, 0));

  MockWrite writes1[] = {
    CreateMockWrite(*req1, 0),
  };

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads1[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*body1, 2),
    MockRead(ASYNC, OK, 3) // EOF
  };

  scoped_ptr<DeterministicSocketData> data1(
      new DeterministicSocketData(reads1, arraysize(reads1),
                                  writes1, arraysize(writes1)));
  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber(ip_addr, &ip));
  IPEndPoint peer_addr = IPEndPoint(ip, 443);
  MockConnect connect_data1(ASYNC, OK, peer_addr);
  data1->set_connect_data(connect_data1);

  // SPDY GET for HTTPS URL (direct)
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(url2.c_str(), false, 1, MEDIUM));

  MockWrite writes2[] = {
    CreateMockWrite(*req2, 0),
  };

  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads2[] = {
    CreateMockRead(*resp2, 1),
    CreateMockRead(*body2, 2),
    MockRead(ASYNC, OK, 3) // EOF
  };

  scoped_ptr<DeterministicSocketData> data2(
      new DeterministicSocketData(reads2, arraysize(reads2),
                                  writes2, arraysize(writes2)));
  MockConnect connect_data2(ASYNC, OK);
  data2->set_connect_data(connect_data2);

  // Set up a proxy config that sends HTTP requests to a proxy, and
  // all others direct.
  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=https://proxy:443");
  CapturingProxyResolver* capturing_proxy_resolver =
      new CapturingProxyResolver();
  session_deps_.proxy_service.reset(new ProxyService(
      new ProxyConfigServiceFixed(proxy_config), capturing_proxy_resolver,
      NULL));

  // Load a valid cert.  Note, that this does not need to
  // be valid for proxy because the MockSSLClientSocket does
  // not actually verify it.  But SpdySession will use this
  // to see if it is valid for the new origin
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> server_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  SSLSocketDataProvider ssl1(ASYNC, OK);  // to the proxy
  ssl1.SetNextProto(GetParam());
  ssl1.cert = server_cert;
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl1);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(
      data1.get());

  SSLSocketDataProvider ssl2(ASYNC, OK);  // to the server
  ssl2.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl2);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(
      data2.get());

  session_deps_.host_resolver.reset(new MockCachingHostResolver());
  session_deps_.host_resolver->rules()->AddRule("mail.google.com", ip_addr);
  session_deps_.host_resolver->rules()->AddRule("proxy", ip_addr);

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_));

  // Start the first transaction to set up the SpdySession
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL(url1);
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(LOWEST, session.get());
  TestCompletionCallback callback1;
  ASSERT_EQ(ERR_IO_PENDING,
            trans1.Start(&request1, callback1.callback(), BoundNetLog()));
  data1->RunFor(3);

  ASSERT_TRUE(callback1.have_result());
  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_TRUE(trans1.GetResponseInfo()->was_fetched_via_spdy);

  // Now, start the HTTP request
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(url2);
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(MEDIUM, session.get());
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            trans2.Start(&request2, callback2.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();
  data2->RunFor(3);

  ASSERT_TRUE(callback2.have_result());
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_TRUE(trans2.GetResponseInfo()->was_fetched_via_spdy);
}

// Test to verify that a failed socket read (due to an ERR_CONNECTION_CLOSED
// error) in SPDY session, removes the socket from pool and closes the SPDY
// session. Verify that new url's from the same HttpNetworkSession (and a new
// SpdySession) do work. http://crbug.com/224701
TEST_P(HttpNetworkTransactionTest, ErrorSocketNotConnected) {
  const std::string https_url = "https://www.google.com/";

  MockRead reads1[] = {
    MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED, 0)
  };

  scoped_ptr<DeterministicSocketData> data1(
      new DeterministicSocketData(reads1, arraysize(reads1), NULL, 0));
  data1->SetStop(1);

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(https_url.c_str(), false, 1, MEDIUM));
  MockWrite writes2[] = {
    CreateMockWrite(*req2, 0),
  };

  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads2[] = {
    CreateMockRead(*resp2, 1),
    CreateMockRead(*body2, 2),
    MockRead(ASYNC, OK, 3)  // EOF
  };

  scoped_ptr<DeterministicSocketData> data2(
      new DeterministicSocketData(reads2, arraysize(reads2),
                                  writes2, arraysize(writes2)));

  SSLSocketDataProvider ssl1(ASYNC, OK);
  ssl1.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl1);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(
      data1.get());

  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.SetNextProto(GetParam());
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl2);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(
      data2.get());

  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_));

  // Start the first transaction to set up the SpdySession and verify that
  // connection was closed.
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL(https_url);
  request1.load_flags = 0;
  HttpNetworkTransaction trans1(MEDIUM, session.get());
  TestCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            trans1.Start(&request1, callback1.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback1.WaitForResult());

  // Now, start the second request and make sure it succeeds.
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL(https_url);
  request2.load_flags = 0;
  HttpNetworkTransaction trans2(MEDIUM, session.get());
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            trans2.Start(&request2, callback2.callback(), BoundNetLog()));
  base::MessageLoop::current()->RunUntilIdle();
  data2->RunFor(3);

  ASSERT_TRUE(callback2.have_result());
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_TRUE(trans2.GetResponseInfo()->was_fetched_via_spdy);
}

TEST_P(HttpNetworkTransactionTest, CloseIdleSpdySessionToOpenNewOne) {
  session_deps_.next_protos = SpdyNextProtos();
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  // Use two different hosts with different IPs so they don't get pooled.
  session_deps_.host_resolver->rules()->AddRule("www.a.com", "10.0.0.1");
  session_deps_.host_resolver->rules()->AddRule("www.b.com", "10.0.0.2");
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  SSLSocketDataProvider ssl1(ASYNC, OK);
  ssl1.SetNextProto(GetParam());
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.SetNextProto(GetParam());
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl1);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl2);

  scoped_ptr<SpdyFrame> host1_req(spdy_util_.ConstructSpdyGet(
      "https://www.a.com", false, 1, DEFAULT_PRIORITY));
  MockWrite spdy1_writes[] = {
    CreateMockWrite(*host1_req, 1),
  };
  scoped_ptr<SpdyFrame> host1_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> host1_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy1_reads[] = {
    CreateMockRead(*host1_resp, 2),
    CreateMockRead(*host1_resp_body, 3),
    MockRead(ASYNC, ERR_IO_PENDING, 4),
  };

  scoped_ptr<OrderedSocketData> spdy1_data(
      new OrderedSocketData(
          spdy1_reads, arraysize(spdy1_reads),
          spdy1_writes, arraysize(spdy1_writes)));
  session_deps_.socket_factory->AddSocketDataProvider(spdy1_data.get());

  scoped_ptr<SpdyFrame> host2_req(spdy_util_.ConstructSpdyGet(
      "https://www.b.com", false, 1, DEFAULT_PRIORITY));
  MockWrite spdy2_writes[] = {
    CreateMockWrite(*host2_req, 1),
  };
  scoped_ptr<SpdyFrame> host2_resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> host2_resp_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy2_reads[] = {
    CreateMockRead(*host2_resp, 2),
    CreateMockRead(*host2_resp_body, 3),
    MockRead(ASYNC, ERR_IO_PENDING, 4),
  };

  scoped_ptr<OrderedSocketData> spdy2_data(
      new OrderedSocketData(
          spdy2_reads, arraysize(spdy2_reads),
          spdy2_writes, arraysize(spdy2_writes)));
  session_deps_.socket_factory->AddSocketDataProvider(spdy2_data.get());

  MockWrite http_write[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.a.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead http_read[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 6\r\n\r\n"),
    MockRead("hello!"),
  };
  StaticSocketDataProvider http_data(http_read, arraysize(http_read),
                                     http_write, arraysize(http_write));
  session_deps_.socket_factory->AddSocketDataProvider(&http_data);

  HostPortPair host_port_pair_a("www.a.com", 443);
  SpdySessionKey spdy_session_key_a(
      host_port_pair_a, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  EXPECT_FALSE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_a));

  TestCompletionCallback callback;
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("https://www.a.com/");
  request1.load_flags = 0;
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  int rv = trans->Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
  trans.reset();
  EXPECT_TRUE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_a));

  HostPortPair host_port_pair_b("www.b.com", 443);
  SpdySessionKey spdy_session_key_b(
      host_port_pair_b, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  EXPECT_FALSE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_b));
  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("https://www.b.com/");
  request2.load_flags = 0;
  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
  EXPECT_FALSE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_a));
  EXPECT_TRUE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_b));

  HostPortPair host_port_pair_a1("www.a.com", 80);
  SpdySessionKey spdy_session_key_a1(
      host_port_pair_a1, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  EXPECT_FALSE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_a1));
  HttpRequestInfo request3;
  request3.method = "GET";
  request3.url = GURL("http://www.a.com/");
  request3.load_flags = 0;
  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));

  rv = trans->Start(&request3, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);
  EXPECT_FALSE(response->was_npn_negotiated);
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
  EXPECT_FALSE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_a));
  EXPECT_FALSE(
      HasSpdySession(session->spdy_session_pool(), spdy_session_key_b));
}

TEST_P(HttpNetworkTransactionTest, HttpSyncConnectError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockConnect mock_connect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider data;
  data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);

  EXPECT_EQ(NULL, trans->GetResponseInfo());

  // We don't care whether this succeeds or fails, but it shouldn't crash.
  HttpRequestHeaders request_headers;
  trans->GetFullRequestHeaders(&request_headers);
}

TEST_P(HttpNetworkTransactionTest, HttpAsyncConnectError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockConnect mock_connect(ASYNC, ERR_CONNECTION_REFUSED);
  StaticSocketDataProvider data;
  data.set_connect_data(mock_connect);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);

  EXPECT_EQ(NULL, trans->GetResponseInfo());

  // We don't care whether this succeeds or fails, but it shouldn't crash.
  HttpRequestHeaders request_headers;
  trans->GetFullRequestHeaders(&request_headers);
}

TEST_P(HttpNetworkTransactionTest, HttpSyncWriteError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };
  MockRead data_reads[] = {
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  EXPECT_EQ(NULL, trans->GetResponseInfo());

  HttpRequestHeaders request_headers;
  EXPECT_TRUE(trans->GetFullRequestHeaders(&request_headers));
  EXPECT_TRUE(request_headers.HasHeader("Host"));
}

TEST_P(HttpNetworkTransactionTest, HttpAsyncWriteError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite(ASYNC, ERR_CONNECTION_RESET),
  };
  MockRead data_reads[] = {
    MockRead(SYNCHRONOUS, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  EXPECT_EQ(NULL, trans->GetResponseInfo());

  HttpRequestHeaders request_headers;
  EXPECT_TRUE(trans->GetFullRequestHeaders(&request_headers));
  EXPECT_TRUE(request_headers.HasHeader("Host"));
}

TEST_P(HttpNetworkTransactionTest, HttpSyncReadError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead data_reads[] = {
    MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  EXPECT_EQ(NULL, trans->GetResponseInfo());

  HttpRequestHeaders request_headers;
  EXPECT_TRUE(trans->GetFullRequestHeaders(&request_headers));
  EXPECT_TRUE(request_headers.HasHeader("Host"));
}

TEST_P(HttpNetworkTransactionTest, HttpAsyncReadError) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead data_reads[] = {
    MockRead(ASYNC, ERR_CONNECTION_RESET),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  EXPECT_EQ(NULL, trans->GetResponseInfo());

  HttpRequestHeaders request_headers;
  EXPECT_TRUE(trans->GetFullRequestHeaders(&request_headers));
  EXPECT_TRUE(request_headers.HasHeader("Host"));
}

TEST_P(HttpNetworkTransactionTest, GetFullRequestHeadersIncludesExtraHeader) {
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  request.extra_headers.SetHeader("X-Foo", "bar");

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "X-Foo: bar\r\n\r\n"),
  };
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Content-Length: 5\r\n\r\n"
             "hello"),
    MockRead(ASYNC, ERR_UNEXPECTED),
  };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  HttpRequestHeaders request_headers;
  EXPECT_TRUE(trans->GetFullRequestHeaders(&request_headers));
  std::string foo;
  EXPECT_TRUE(request_headers.GetHeader("X-Foo", &foo));
  EXPECT_EQ("bar", foo);
}

namespace {

// Fake HttpStreamBase that simply records calls to SetPriority().
class FakeStream : public HttpStreamBase,
                   public base::SupportsWeakPtr<FakeStream> {
 public:
  explicit FakeStream(RequestPriority priority) : priority_(priority) {}
  virtual ~FakeStream() {}

  RequestPriority priority() const { return priority_; }

  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               RequestPriority priority,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) OVERRIDE {
    return ERR_IO_PENDING;
  }

  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) OVERRIDE {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual int ReadResponseHeaders(const CompletionCallback& callback) OVERRIDE {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) OVERRIDE {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual void Close(bool not_reusable) OVERRIDE {}

  virtual bool IsResponseBodyComplete() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual bool CanFindEndOfResponse() const OVERRIDE {
    return false;
  }

  virtual bool IsConnectionReused() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual void SetConnectionReused() OVERRIDE {
    ADD_FAILURE();
  }

  virtual bool IsConnectionReusable() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual int64 GetTotalReceivedBytes() const OVERRIDE {
    ADD_FAILURE();
    return 0;
  }

  virtual bool GetLoadTimingInfo(
      LoadTimingInfo* load_timing_info) const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual void GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE {
    ADD_FAILURE();
  }

  virtual bool IsSpdyHttpStream() const OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual void Drain(HttpNetworkSession* session) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void SetPriority(RequestPriority priority) OVERRIDE {
    priority_ = priority;
  }

 private:
  RequestPriority priority_;

  DISALLOW_COPY_AND_ASSIGN(FakeStream);
};

// Fake HttpStreamRequest that simply records calls to SetPriority()
// and vends FakeStreams with its current priority.
class FakeStreamRequest : public HttpStreamRequest,
                          public base::SupportsWeakPtr<FakeStreamRequest> {
 public:
  FakeStreamRequest(RequestPriority priority,
                    HttpStreamRequest::Delegate* delegate)
      : priority_(priority),
        delegate_(delegate),
        websocket_stream_create_helper_(NULL) {}

  FakeStreamRequest(RequestPriority priority,
                    HttpStreamRequest::Delegate* delegate,
                    WebSocketHandshakeStreamBase::CreateHelper* create_helper)
      : priority_(priority),
        delegate_(delegate),
        websocket_stream_create_helper_(create_helper) {}

  virtual ~FakeStreamRequest() {}

  RequestPriority priority() const { return priority_; }

  const WebSocketHandshakeStreamBase::CreateHelper*
  websocket_stream_create_helper() const {
    return websocket_stream_create_helper_;
  }

  // Create a new FakeStream and pass it to the request's
  // delegate. Returns a weak pointer to the FakeStream.
  base::WeakPtr<FakeStream> FinishStreamRequest() {
    FakeStream* fake_stream = new FakeStream(priority_);
    // Do this before calling OnStreamReady() as OnStreamReady() may
    // immediately delete |fake_stream|.
    base::WeakPtr<FakeStream> weak_stream = fake_stream->AsWeakPtr();
    delegate_->OnStreamReady(SSLConfig(), ProxyInfo(), fake_stream);
    return weak_stream;
  }

  virtual int RestartTunnelWithProxyAuth(
      const AuthCredentials& credentials) OVERRIDE {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual LoadState GetLoadState() const OVERRIDE {
    ADD_FAILURE();
    return LoadState();
  }

  virtual void SetPriority(RequestPriority priority) OVERRIDE {
    priority_ = priority;
  }

  virtual bool was_npn_negotiated() const OVERRIDE {
    return false;
  }

  virtual NextProto protocol_negotiated() const OVERRIDE {
    return kProtoUnknown;
  }

  virtual bool using_spdy() const OVERRIDE {
    return false;
  }

 private:
  RequestPriority priority_;
  HttpStreamRequest::Delegate* const delegate_;
  WebSocketHandshakeStreamBase::CreateHelper* websocket_stream_create_helper_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamRequest);
};

// Fake HttpStreamFactory that vends FakeStreamRequests.
class FakeStreamFactory : public HttpStreamFactory {
 public:
  FakeStreamFactory() {}
  virtual ~FakeStreamFactory() {}

  // Returns a WeakPtr<> to the last HttpStreamRequest returned by
  // RequestStream() (which may be NULL if it was destroyed already).
  base::WeakPtr<FakeStreamRequest> last_stream_request() {
    return last_stream_request_;
  }

  virtual HttpStreamRequest* RequestStream(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HttpStreamRequest::Delegate* delegate,
      const BoundNetLog& net_log) OVERRIDE {
    FakeStreamRequest* fake_request = new FakeStreamRequest(priority, delegate);
    last_stream_request_ = fake_request->AsWeakPtr();
    return fake_request;
  }

  virtual HttpStreamRequest* RequestWebSocketHandshakeStream(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HttpStreamRequest::Delegate* delegate,
      WebSocketHandshakeStreamBase::CreateHelper* create_helper,
      const BoundNetLog& net_log) OVERRIDE {
    FakeStreamRequest* fake_request =
        new FakeStreamRequest(priority, delegate, create_helper);
    last_stream_request_ = fake_request->AsWeakPtr();
    return fake_request;
  }

  virtual void PreconnectStreams(int num_streams,
                                 const HttpRequestInfo& info,
                                 RequestPriority priority,
                                 const SSLConfig& server_ssl_config,
                                 const SSLConfig& proxy_ssl_config) OVERRIDE {
    ADD_FAILURE();
  }

  virtual const HostMappingRules* GetHostMappingRules() const OVERRIDE {
    ADD_FAILURE();
    return NULL;
  }

 private:
  base::WeakPtr<FakeStreamRequest> last_stream_request_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamFactory);
};

// TODO(yhirano): Split this class out into a net/websockets file, if it is
// worth doing.
class FakeWebSocketStreamCreateHelper :
      public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  virtual WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<ClientSocketHandle> connection,
      bool using_proxy) OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual WebSocketHandshakeStreamBase* CreateSpdyStream(
      const base::WeakPtr<SpdySession>& session,
      bool use_relative_url) OVERRIDE {
    NOTREACHED();
    return NULL;
  };

  virtual ~FakeWebSocketStreamCreateHelper() {}

  virtual scoped_ptr<WebSocketStream> Upgrade() {
    NOTREACHED();
    return scoped_ptr<WebSocketStream>();
  }
};

}  // namespace

// Make sure that HttpNetworkTransaction passes on its priority to its
// stream request on start.
TEST_P(HttpNetworkTransactionTest, SetStreamRequestPriorityOnStart) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  HttpNetworkSessionPeer peer(session);
  FakeStreamFactory* fake_factory = new FakeStreamFactory();
  peer.SetHttpStreamFactory(scoped_ptr<HttpStreamFactory>(fake_factory));

  HttpNetworkTransaction trans(LOW, session);

  ASSERT_TRUE(fake_factory->last_stream_request() == NULL);

  HttpRequestInfo request;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            trans.Start(&request, callback.callback(), BoundNetLog()));

  base::WeakPtr<FakeStreamRequest> fake_request =
      fake_factory->last_stream_request();
  ASSERT_TRUE(fake_request != NULL);
  EXPECT_EQ(LOW, fake_request->priority());
}

// Make sure that HttpNetworkTransaction passes on its priority
// updates to its stream request.
TEST_P(HttpNetworkTransactionTest, SetStreamRequestPriority) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  HttpNetworkSessionPeer peer(session);
  FakeStreamFactory* fake_factory = new FakeStreamFactory();
  peer.SetHttpStreamFactory(scoped_ptr<HttpStreamFactory>(fake_factory));

  HttpNetworkTransaction trans(LOW, session);

  HttpRequestInfo request;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            trans.Start(&request, callback.callback(), BoundNetLog()));

  base::WeakPtr<FakeStreamRequest> fake_request =
      fake_factory->last_stream_request();
  ASSERT_TRUE(fake_request != NULL);
  EXPECT_EQ(LOW, fake_request->priority());

  trans.SetPriority(LOWEST);
  ASSERT_TRUE(fake_request != NULL);
  EXPECT_EQ(LOWEST, fake_request->priority());
}

// Make sure that HttpNetworkTransaction passes on its priority
// updates to its stream.
TEST_P(HttpNetworkTransactionTest, SetStreamPriority) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  HttpNetworkSessionPeer peer(session);
  FakeStreamFactory* fake_factory = new FakeStreamFactory();
  peer.SetHttpStreamFactory(scoped_ptr<HttpStreamFactory>(fake_factory));

  HttpNetworkTransaction trans(LOW, session);

  HttpRequestInfo request;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            trans.Start(&request, callback.callback(), BoundNetLog()));

  base::WeakPtr<FakeStreamRequest> fake_request =
      fake_factory->last_stream_request();
  ASSERT_TRUE(fake_request != NULL);
  base::WeakPtr<FakeStream> fake_stream = fake_request->FinishStreamRequest();
  ASSERT_TRUE(fake_stream != NULL);
  EXPECT_EQ(LOW, fake_stream->priority());

  trans.SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, fake_stream->priority());
}

TEST_P(HttpNetworkTransactionTest, CreateWebSocketHandshakeStream) {
  // The same logic needs to be tested for both ws: and wss: schemes, but this
  // test is already parameterised on NextProto, so it uses a loop to verify
  // that the different schemes work.
  std::string test_cases[] = {"ws://www.google.com/", "wss://www.google.com/"};
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
    HttpNetworkSessionPeer peer(session);
    FakeStreamFactory* fake_factory = new FakeStreamFactory();
    FakeWebSocketStreamCreateHelper websocket_stream_create_helper;
    peer.SetHttpStreamFactoryForWebSocket(
        scoped_ptr<HttpStreamFactory>(fake_factory));

    HttpNetworkTransaction trans(LOW, session);
    trans.SetWebSocketHandshakeStreamCreateHelper(
        &websocket_stream_create_helper);

    HttpRequestInfo request;
    TestCompletionCallback callback;
    request.method = "GET";
    request.url = GURL(test_cases[i]);

    EXPECT_EQ(ERR_IO_PENDING,
              trans.Start(&request, callback.callback(), BoundNetLog()));

    base::WeakPtr<FakeStreamRequest> fake_request =
        fake_factory->last_stream_request();
    ASSERT_TRUE(fake_request != NULL);
    EXPECT_EQ(&websocket_stream_create_helper,
              fake_request->websocket_stream_create_helper());
  }
}

// Tests that when a used socket is returned to the SSL socket pool, it's closed
// if the transport socket pool is stalled on the global socket limit.
TEST_P(HttpNetworkTransactionTest, CloseSSLSocketOnIdleForHttpRequest) {
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  // Set up SSL request.

  HttpRequestInfo ssl_request;
  ssl_request.method = "GET";
  ssl_request.url = GURL("https://www.google.com/");

  MockWrite ssl_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead ssl_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 11\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider ssl_data(ssl_reads, arraysize(ssl_reads),
                                    ssl_writes, arraysize(ssl_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&ssl_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  // Set up HTTP request.

  HttpRequestInfo http_request;
  http_request.method = "GET";
  http_request.url = GURL("http://www.google.com/");

  MockWrite http_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 7\r\n\r\n"),
    MockRead("falafel"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&http_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Start the SSL request.
  TestCompletionCallback ssl_callback;
  scoped_ptr<HttpTransaction> ssl_trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  ASSERT_EQ(ERR_IO_PENDING,
            ssl_trans->Start(&ssl_request, ssl_callback.callback(),
            BoundNetLog()));

  // Start the HTTP request.  Pool should stall.
  TestCompletionCallback http_callback;
  scoped_ptr<HttpTransaction> http_trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  ASSERT_EQ(ERR_IO_PENDING,
            http_trans->Start(&http_request, http_callback.callback(),
                              BoundNetLog()));
  EXPECT_TRUE(IsTransportSocketPoolStalled(session));

  // Wait for response from SSL request.
  ASSERT_EQ(OK, ssl_callback.WaitForResult());
  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(ssl_trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  // The SSL socket should automatically be closed, so the HTTP request can
  // start.
  EXPECT_EQ(0, GetIdleSocketCountInSSLSocketPool(session));
  ASSERT_FALSE(IsTransportSocketPoolStalled(session));

  // The HTTP request can now complete.
  ASSERT_EQ(OK, http_callback.WaitForResult());
  ASSERT_EQ(OK, ReadTransaction(http_trans.get(), &response_data));
  EXPECT_EQ("falafel", response_data);

  EXPECT_EQ(1, GetIdleSocketCountInTransportSocketPool(session));
}

// Tests that when a SSL connection is established but there's no corresponding
// request that needs it, the new socket is closed if the transport socket pool
// is stalled on the global socket limit.
TEST_P(HttpNetworkTransactionTest, CloseSSLSocketOnIdleForHttpRequest2) {
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  // Set up an ssl request.

  HttpRequestInfo ssl_request;
  ssl_request.method = "GET";
  ssl_request.url = GURL("https://www.foopy.com/");

  // No data will be sent on the SSL socket.
  StaticSocketDataProvider ssl_data;
  session_deps_.socket_factory->AddSocketDataProvider(&ssl_data);

  SSLSocketDataProvider ssl(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  // Set up HTTP request.

  HttpRequestInfo http_request;
  http_request.method = "GET";
  http_request.url = GURL("http://www.google.com/");

  MockWrite http_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Length: 7\r\n\r\n"),
    MockRead("falafel"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider http_data(http_reads, arraysize(http_reads),
                                     http_writes, arraysize(http_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&http_data);

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));

  // Preconnect an SSL socket.  A preconnect is needed because connect jobs are
  // cancelled when a normal transaction is cancelled.
  net::HttpStreamFactory* http_stream_factory = session->http_stream_factory();
  net::SSLConfig ssl_config;
  session->ssl_config_service()->GetSSLConfig(&ssl_config);
  http_stream_factory->PreconnectStreams(1, ssl_request, DEFAULT_PRIORITY,
                                         ssl_config, ssl_config);
  EXPECT_EQ(0, GetIdleSocketCountInSSLSocketPool(session));

  // Start the HTTP request.  Pool should stall.
  TestCompletionCallback http_callback;
  scoped_ptr<HttpTransaction> http_trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  ASSERT_EQ(ERR_IO_PENDING,
            http_trans->Start(&http_request, http_callback.callback(),
                              BoundNetLog()));
  EXPECT_TRUE(IsTransportSocketPoolStalled(session));

  // The SSL connection will automatically be closed once the connection is
  // established, to let the HTTP request start.
  ASSERT_EQ(OK, http_callback.WaitForResult());
  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(http_trans.get(), &response_data));
  EXPECT_EQ("falafel", response_data);

  EXPECT_EQ(1, GetIdleSocketCountInTransportSocketPool(session));
}

TEST_P(HttpNetworkTransactionTest, PostReadsErrorResponseAfterReset) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 400 Not OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 400 Not OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// This test makes sure the retry logic doesn't trigger when reading an error
// response from a server that rejected a POST with a CONNECTION_RESET.
TEST_P(HttpNetworkTransactionTest,
       PostReadsErrorResponseAfterResetOnReusedSocket) {
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 Peachy\r\n"
             "Content-Length: 14\r\n\r\n"),
    MockRead("first response"),
    MockRead("HTTP/1.1 400 Not OK\r\n"
             "Content-Length: 15\r\n\r\n"),
    MockRead("second response"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;
  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("http://www.foo.com/");
  request1.load_flags = 0;

  scoped_ptr<HttpTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  int rv = trans1->Start(&request1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);

  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 Peachy", response1->headers->GetStatusLine());

  std::string response_data1;
  rv = ReadTransaction(trans1.get(), &response_data1);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("first response", response_data1);
  // Delete the transaction to release the socket back into the socket pool.
  trans1.reset();

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request2;
  request2.method = "POST";
  request2.url = GURL("http://www.foo.com/");
  request2.upload_data_stream = &upload_data_stream;
  request2.load_flags = 0;

  scoped_ptr<HttpTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  rv = trans2->Start(&request2, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);

  EXPECT_TRUE(response2->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 400 Not OK", response2->headers->GetStatusLine());

  std::string response_data2;
  rv = ReadTransaction(trans2.get(), &response_data2);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("second response", response_data2);
}

TEST_P(HttpNetworkTransactionTest,
       PostReadsErrorResponseAfterResetPartialBodySent) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"
              "fo"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 400 Not OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 400 Not OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// This tests the more common case than the previous test, where headers and
// body are not merged into a single request.
TEST_P(HttpNetworkTransactionTest, ChunkedPostReadsErrorResponseAfterReset) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(UploadDataStream::CHUNKED, 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Transfer-Encoding: chunked\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 400 Not OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Make sure the headers are sent before adding a chunk.  This ensures that
  // they can't be merged with the body in a single send.  Not currently
  // necessary since a chunked body is never merged with headers, but this makes
  // the test more future proof.
  base::RunLoop().RunUntilIdle();

  upload_data_stream.AppendChunk("last chunk", 10, true);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 400 Not OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

TEST_P(HttpNetworkTransactionTest, PostReadsErrorResponseAfterResetAnd100) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));

  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 100 Continue\r\n\r\n"),
    MockRead("HTTP/1.0 400 Not OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.0 400 Not OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

TEST_P(HttpNetworkTransactionTest, PostIgnoresNonErrorResponseAfterReset) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 Just Dandy\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

TEST_P(HttpNetworkTransactionTest,
       PostIgnoresNonErrorResponseAfterResetAnd100) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 100 Continue\r\n\r\n"),
    MockRead("HTTP/1.0 302 Redirect\r\n"),
    MockRead("Location: http://somewhere-else.com/\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

TEST_P(HttpNetworkTransactionTest, PostIgnoresHttp09ResponseAfterReset) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP 0.9 rocks!"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

TEST_P(HttpNetworkTransactionTest, PostIgnoresPartial400HeadersAfterReset) {
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadBytesElementReader("foo", 3));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data_stream = &upload_data_stream;
  request.load_flags = 0;

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps_));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session));
  // Send headers successfully, but get an error while sending the body.
  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.foo.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 3\r\n\r\n"),
    MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 400 Not a Full Response\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads), data_writes,
                                arraysize(data_writes));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

}  // namespace net
