// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

// Make sure |handle|'s load times are set correctly.  DNS and connect start
// times comes from mock client sockets in these tests, so primarily serves to
// check those times were copied, and ssl times / connect end are set correctly.
void TestLoadTimingInfo(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  // None of these tests use a NetLog.
  EXPECT_EQ(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(
      load_timing_info.connect_timing,
      CONNECT_TIMING_HAS_SSL_TIMES | CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Just like TestLoadTimingInfo, except DNS times are expected to be null, for
// tests over proxies that do DNS lookups themselves.
void TestLoadTimingInfoNoDns(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  // None of these tests use a NetLog.
  EXPECT_EQ(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.socket_reused);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_SSL_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

class SSLClientSocketPoolTest
    : public testing::Test,
      public ::testing::WithParamInterface<NextProto> {
 protected:
  SSLClientSocketPoolTest()
      : proxy_service_(ProxyService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        http_auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        session_(CreateNetworkSession()),
        direct_transport_socket_params_(
            new TransportSocketParams(HostPortPair("host", 443),
                                      false,
                                      false,
                                      OnHostResolutionCallback())),
        transport_histograms_("MockTCP"),
        transport_socket_pool_(kMaxSockets,
                               kMaxSocketsPerGroup,
                               &transport_histograms_,
                               &socket_factory_),
        proxy_transport_socket_params_(
            new TransportSocketParams(HostPortPair("proxy", 443),
                                      false,
                                      false,
                                      OnHostResolutionCallback())),
        socks_socket_params_(
            new SOCKSSocketParams(proxy_transport_socket_params_,
                                  true,
                                  HostPortPair("sockshost", 443))),
        socks_histograms_("MockSOCKS"),
        socks_socket_pool_(kMaxSockets,
                           kMaxSocketsPerGroup,
                           &socks_histograms_,
                           &transport_socket_pool_),
        http_proxy_socket_params_(
            new HttpProxySocketParams(proxy_transport_socket_params_,
                                      NULL,
                                      GURL("http://host"),
                                      std::string(),
                                      HostPortPair("host", 80),
                                      session_->http_auth_cache(),
                                      session_->http_auth_handler_factory(),
                                      session_->spdy_session_pool(),
                                      true)),
        http_proxy_histograms_("MockHttpProxy"),
        http_proxy_socket_pool_(kMaxSockets,
                                kMaxSocketsPerGroup,
                                &http_proxy_histograms_,
                                &host_resolver_,
                                &transport_socket_pool_,
                                NULL,
                                NULL) {
    scoped_refptr<SSLConfigService> ssl_config_service(
        new SSLConfigServiceDefaults);
    ssl_config_service->GetSSLConfig(&ssl_config_);
  }

  void CreatePool(bool transport_pool, bool http_proxy_pool, bool socks_pool) {
    ssl_histograms_.reset(new ClientSocketPoolHistograms("SSLUnitTest"));
    pool_.reset(new SSLClientSocketPool(
        kMaxSockets,
        kMaxSocketsPerGroup,
        ssl_histograms_.get(),
        NULL /* host_resolver */,
        NULL /* cert_verifier */,
        NULL /* server_bound_cert_service */,
        NULL /* transport_security_state */,
        NULL /* cert_transparency_verifier */,
        std::string() /* ssl_session_cache_shard */,
        &socket_factory_,
        transport_pool ? &transport_socket_pool_ : NULL,
        socks_pool ? &socks_socket_pool_ : NULL,
        http_proxy_pool ? &http_proxy_socket_pool_ : NULL,
        NULL,
        NULL));
  }

  scoped_refptr<SSLSocketParams> SSLParams(ProxyServer::Scheme proxy,
                                           bool want_spdy_over_npn) {
    return make_scoped_refptr(new SSLSocketParams(
        proxy == ProxyServer::SCHEME_DIRECT ? direct_transport_socket_params_
                                            : NULL,
        proxy == ProxyServer::SCHEME_SOCKS5 ? socks_socket_params_ : NULL,
        proxy == ProxyServer::SCHEME_HTTP ? http_proxy_socket_params_ : NULL,
        HostPortPair("host", 443),
        ssl_config_,
        PRIVACY_MODE_DISABLED,
        0,
        false,
        want_spdy_over_npn));
  }

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL("http://proxy:443/"),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  HttpNetworkSession* CreateNetworkSession() {
    HttpNetworkSession::Params params;
    params.host_resolver = &host_resolver_;
    params.cert_verifier = cert_verifier_.get();
    params.transport_security_state = transport_security_state_.get();
    params.proxy_service = proxy_service_.get();
    params.client_socket_factory = &socket_factory_;
    params.ssl_config_service = ssl_config_service_.get();
    params.http_auth_handler_factory = http_auth_handler_factory_.get();
    params.http_server_properties =
        http_server_properties_.GetWeakPtr();
    params.enable_spdy_compression = false;
    params.spdy_default_protocol = GetParam();
    return new HttpNetworkSession(params);
  }

  void TestIPPoolingDisabled(SSLSocketDataProvider* ssl);

  MockClientSocketFactory socket_factory_;
  MockCachingHostResolver host_resolver_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
  const scoped_ptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  const scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  HttpServerPropertiesImpl http_server_properties_;
  const scoped_refptr<HttpNetworkSession> session_;

  scoped_refptr<TransportSocketParams> direct_transport_socket_params_;
  ClientSocketPoolHistograms transport_histograms_;
  MockTransportClientSocketPool transport_socket_pool_;

  scoped_refptr<TransportSocketParams> proxy_transport_socket_params_;

  scoped_refptr<SOCKSSocketParams> socks_socket_params_;
  ClientSocketPoolHistograms socks_histograms_;
  MockSOCKSClientSocketPool socks_socket_pool_;

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_;
  ClientSocketPoolHistograms http_proxy_histograms_;
  HttpProxyClientSocketPool http_proxy_socket_pool_;

  SSLConfig ssl_config_;
  scoped_ptr<ClientSocketPoolHistograms> ssl_histograms_;
  scoped_ptr<SSLClientSocketPool> pool_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SSLClientSocketPoolTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

TEST_P(SSLClientSocketPoolTest, TCPFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  int rv = handle.Init("a", params, MEDIUM, CompletionCallback(), pool_.get(),
                       BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, TCPFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, BasicDirect) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
}

// Make sure that SSLConnectJob passes on its priority to its
// socket request on Init (for the DIRECT case).
TEST_P(SSLClientSocketPoolTest, SetSocketRequestPriorityOnInitDirect) {
  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params =
      SSLParams(ProxyServer::SCHEME_DIRECT, false);

  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    RequestPriority priority = static_cast<RequestPriority>(i);
    StaticSocketDataProvider data;
    data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
    socket_factory_.AddSocketDataProvider(&data);
    SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
    socket_factory_.AddSSLSocketDataProvider(&ssl);

    ClientSocketHandle handle;
    TestCompletionCallback callback;
    EXPECT_EQ(OK, handle.Init("a", params, priority, callback.callback(),
                              pool_.get(), BoundNetLog()));
    EXPECT_EQ(priority, transport_socket_pool_.last_request_priority());
    handle.socket()->Disconnect();
  }
}

TEST_P(SSLClientSocketPoolTest, BasicDirectAsync) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
}

TEST_P(SSLClientSocketPoolTest, DirectCertError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_COMMON_NAME_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
}

TEST_P(SSLClientSocketPoolTest, DirectSSLError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, DirectWithNPN) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(kProtoHTTP11);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->WasNpnNegotiated());
}

TEST_P(SSLClientSocketPoolTest, DirectNoSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(kProtoHTTP11);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_NPN_NEGOTIATION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, DirectGotSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);

  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->WasNpnNegotiated());
  std::string proto;
  std::string server_protos;
  ssl_socket->GetNextProto(&proto, &server_protos);
  EXPECT_EQ(GetParam(), SSLClientSocket::NextProtoFromString(proto));
}

TEST_P(SSLClientSocketPoolTest, DirectGotBonusSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.SetNextProto(GetParam());
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);

  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->WasNpnNegotiated());
  std::string proto;
  std::string server_protos;
  ssl_socket->GetNextProto(&proto, &server_protos);
  EXPECT_EQ(GetParam(), SSLClientSocket::NextProtoFromString(proto));
}

TEST_P(SSLClientSocketPoolTest, SOCKSFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, SOCKSFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, SOCKSBasic) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  // SOCKS5 generally has no DNS times, but the mock SOCKS5 sockets used here
  // don't go through the real logic, unlike in the HTTP proxy tests.
  TestLoadTimingInfo(handle);
}

// Make sure that SSLConnectJob passes on its priority to its
// transport socket on Init (for the SOCKS_PROXY case).
TEST_P(SSLClientSocketPoolTest, SetTransportPriorityOnInitSOCKS) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params =
      SSLParams(ProxyServer::SCHEME_SOCKS5, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(OK, handle.Init("a", params, HIGHEST, callback.callback(),
                            pool_.get(), BoundNetLog()));
  EXPECT_EQ(HIGHEST, transport_socket_pool_.last_request_priority());
}

TEST_P(SSLClientSocketPoolTest, SOCKSBasicAsync) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  // SOCKS5 generally has no DNS times, but the mock SOCKS5 sockets used here
  // don't go through the real logic, unlike in the HTTP proxy tests.
  TestLoadTimingInfo(handle);
}

TEST_P(SSLClientSocketPoolTest, HttpProxyFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, HttpProxyFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_P(SSLClientSocketPoolTest, HttpProxyBasic) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoNoDns(handle);
}

// Make sure that SSLConnectJob passes on its priority to its
// transport socket on Init (for the HTTP_PROXY case).
TEST_P(SSLClientSocketPoolTest, SetTransportPriorityOnInitHTTP) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params =
      SSLParams(ProxyServer::SCHEME_HTTP, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(OK, handle.Init("a", params, HIGHEST, callback.callback(),
                            pool_.get(), BoundNetLog()));
  EXPECT_EQ(HIGHEST, transport_socket_pool_.last_request_priority());
}

TEST_P(SSLClientSocketPoolTest, HttpProxyBasicAsync) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoNoDns(handle);
}

TEST_P(SSLClientSocketPoolTest, NeedProxyAuth) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10\r\n\r\n"),
      MockRead("0123456789"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, callback.callback(), pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);
  scoped_ptr<ClientSocketHandle> tunnel_handle(
      handle.release_pending_http_proxy_connection());
  EXPECT_TRUE(tunnel_handle->socket());
  EXPECT_FALSE(tunnel_handle->socket()->IsConnected());
}

// TODO(rch): re-enable this.
TEST_P(SSLClientSocketPoolTest, DISABLED_IPPooling) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
    { "www.webkit.org",    "192.0.2.33,192.168.0.1,192.168.0.5" },
    { "code.google.com",   "192.168.0.2,192.168.0.3,192.168.0.5" },
    { "js.webkit.org",     "192.168.0.4,192.168.0.1,192.0.2.33" },
  };

  host_resolver_.set_synchronous_mode(true);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_hosts); i++) {
    host_resolver_.rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    host_resolver_.Resolve(info,
                           DEFAULT_PRIORITY,
                           &test_hosts[i].addresses,
                           CompletionCallback(),
                           NULL,
                           BoundNetLog());

    // Setup a SpdySessionKey
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED);
  }

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der));
  ssl.SetNextProto(GetParam());
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  base::WeakPtr<SpdySession> spdy_session =
      CreateSecureSpdySession(session_, test_hosts[0].key, BoundNetLog());

  EXPECT_TRUE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[0].key));
  EXPECT_FALSE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[1].key));
  EXPECT_TRUE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[2].key));

  session_->spdy_session_pool()->CloseAllSessions();
}

void SSLClientSocketPoolTest::TestIPPoolingDisabled(
    SSLSocketDataProvider* ssl) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
    { "www.webkit.org",    "192.0.2.33,192.168.0.1,192.168.0.5" },
    { "js.webkit.com",     "192.168.0.4,192.168.0.1,192.0.2.33" },
  };

  TestCompletionCallback callback;
  int rv;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_hosts); i++) {
    host_resolver_.rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    rv = host_resolver_.Resolve(info,
                                DEFAULT_PRIORITY,
                                &test_hosts[i].addresses,
                                callback.callback(),
                                NULL,
                                BoundNetLog());
    EXPECT_EQ(OK, callback.GetResult(rv));

    // Setup a SpdySessionKey
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED);
  }

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&data);
  socket_factory_.AddSSLSocketDataProvider(ssl);

  CreatePool(true /* tcp pool */, false, false);
  base::WeakPtr<SpdySession> spdy_session =
      CreateSecureSpdySession(session_, test_hosts[0].key, BoundNetLog());

  EXPECT_TRUE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[0].key));
  EXPECT_FALSE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[1].key));

  session_->spdy_session_pool()->CloseAllSessions();
}

// Verifies that an SSL connection with client authentication disables SPDY IP
// pooling.
TEST_P(SSLClientSocketPoolTest, IPPoolingClientCert) {
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der));
  ssl.client_cert_sent = true;
  ssl.SetNextProto(GetParam());
  TestIPPoolingDisabled(&ssl);
}

// Verifies that an SSL connection with channel ID disables SPDY IP pooling.
TEST_P(SSLClientSocketPoolTest, IPPoolingChannelID) {
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.channel_id_sent = true;
  ssl.SetNextProto(GetParam());
  TestIPPoolingDisabled(&ssl);
}

// It would be nice to also test the timeouts in SSLClientSocketPool.

}  // namespace

}  // namespace net
