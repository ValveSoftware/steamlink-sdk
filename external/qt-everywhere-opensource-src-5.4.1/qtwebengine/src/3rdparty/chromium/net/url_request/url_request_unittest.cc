// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shlobj.h>
#endif

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/capturing_net_log.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_data_directory.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(DISABLE_FILE_SUPPORT)
#include "net/base/filename_util.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_file_dir_job.h"
#endif

#if !defined(DISABLE_FTP_SUPPORT)
#include "net/url_request/ftp_protocol_handler.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#endif

using base::ASCIIToUTF16;
using base::Time;

namespace net {

namespace {

const base::string16 kChrome(ASCIIToUTF16("chrome"));
const base::string16 kSecret(ASCIIToUTF16("secret"));
const base::string16 kUser(ASCIIToUTF16("user"));

// Tests load timing information in the case a fresh connection was used, with
// no proxy.
void TestLoadTimingNotReused(const net::LoadTimingInfo& load_timing_info,
                             int connect_timing_flags) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.connect_timing.connect_start);
  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              connect_timing_flags);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
}

// Same as above, but with proxy times.
void TestLoadTimingNotReusedWithProxy(
    const net::LoadTimingInfo& load_timing_info,
    int connect_timing_flags) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.proxy_resolve_start);
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.connect_timing.connect_start);
  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              connect_timing_flags);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);
}

// Same as above, but with a reused socket and proxy times.
void TestLoadTimingReusedWithProxy(
    const net::LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.proxy_resolve_start);
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);
}

// Tests load timing information in the case of a cache hit, when no cache
// validation request was sent over the wire.
base::StringPiece TestNetResourceProvider(int key) {
  return "header";
}

void FillBuffer(char* buffer, size_t len) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i])
      buffer[i] = 'g';
  }
}

#if !defined(OS_IOS)
void TestLoadTimingCacheHitNoNetwork(
    const net::LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  EXPECT_LE(load_timing_info.request_start, load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
}

// Tests load timing in the case that there is no HTTP response.  This can be
// used to test in the case of errors or non-HTTP requests.
void TestLoadTimingNoHttpResponse(
    const net::LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  // Only the request times should be non-null.
  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
  EXPECT_TRUE(load_timing_info.send_start.is_null());
  EXPECT_TRUE(load_timing_info.send_end.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Do a case-insensitive search through |haystack| for |needle|.
bool ContainsString(const std::string& haystack, const char* needle) {
  std::string::const_iterator it =
      std::search(haystack.begin(),
                  haystack.end(),
                  needle,
                  needle + strlen(needle),
                  base::CaseInsensitiveCompare<char>());
  return it != haystack.end();
}

UploadDataStream* CreateSimpleUploadData(const char* data) {
  scoped_ptr<UploadElementReader> reader(
      new UploadBytesElementReader(data, strlen(data)));
  return UploadDataStream::CreateWithReader(reader.Pass(), 0);
}

// Verify that the SSLInfo of a successful SSL connection has valid values.
void CheckSSLInfo(const SSLInfo& ssl_info) {
  // -1 means unknown.  0 means no encryption.
  EXPECT_GT(ssl_info.security_bits, 0);

  // The cipher suite TLS_NULL_WITH_NULL_NULL (0) must not be negotiated.
  int cipher_suite = SSLConnectionStatusToCipherSuite(
      ssl_info.connection_status);
  EXPECT_NE(0, cipher_suite);
}

void CheckFullRequestHeaders(const HttpRequestHeaders& headers,
                             const GURL& host_url) {
  std::string sent_value;

  EXPECT_TRUE(headers.GetHeader("Host", &sent_value));
  EXPECT_EQ(GetHostAndOptionalPort(host_url), sent_value);

  EXPECT_TRUE(headers.GetHeader("Connection", &sent_value));
  EXPECT_EQ("keep-alive", sent_value);
}

bool FingerprintsEqual(const HashValueVector& a, const HashValueVector& b) {
  size_t size = a.size();

  if (size != b.size())
    return false;

  for (size_t i = 0; i < size; ++i) {
    if (!a[i].Equals(b[i]))
      return false;
  }

  return true;
}
#endif  // !defined(OS_IOS)

// A network delegate that allows the user to choose a subset of request stages
// to block in. When blocking, the delegate can do one of the following:
//  * synchronously return a pre-specified error code, or
//  * asynchronously return that value via an automatically called callback,
//    or
//  * block and wait for the user to do a callback.
// Additionally, the user may also specify a redirect URL -- then each request
// with the current URL different from the redirect target will be redirected
// to that target, in the on-before-URL-request stage, independent of whether
// the delegate blocks in ON_BEFORE_URL_REQUEST or not.
class BlockingNetworkDelegate : public TestNetworkDelegate {
 public:
  // Stages in which the delegate can block.
  enum Stage {
    NOT_BLOCKED = 0,
    ON_BEFORE_URL_REQUEST = 1 << 0,
    ON_BEFORE_SEND_HEADERS = 1 << 1,
    ON_HEADERS_RECEIVED = 1 << 2,
    ON_AUTH_REQUIRED = 1 << 3
  };

  // Behavior during blocked stages.  During other stages, just
  // returns net::OK or NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION.
  enum BlockMode {
    SYNCHRONOUS,    // No callback, returns specified return values.
    AUTO_CALLBACK,  // |this| posts a task to run the callback using the
                    // specified return codes.
    USER_CALLBACK,  // User takes care of doing a callback.  |retval_| and
                    // |auth_retval_| are ignored. In every blocking stage the
                    // message loop is quit.
  };

  // Creates a delegate which does not block at all.
  explicit BlockingNetworkDelegate(BlockMode block_mode);

  // For users to trigger a callback returning |response|.
  // Side-effects: resets |stage_blocked_for_callback_| and stored callbacks.
  // Only call if |block_mode_| == USER_CALLBACK.
  void DoCallback(int response);
  void DoAuthCallback(NetworkDelegate::AuthRequiredResponse response);

  // Setters.
  void set_retval(int retval) {
    ASSERT_NE(USER_CALLBACK, block_mode_);
    ASSERT_NE(ERR_IO_PENDING, retval);
    ASSERT_NE(OK, retval);
    retval_ = retval;
  }

  // If |auth_retval| == AUTH_REQUIRED_RESPONSE_SET_AUTH, then
  // |auth_credentials_| will be passed with the response.
  void set_auth_retval(AuthRequiredResponse auth_retval) {
    ASSERT_NE(USER_CALLBACK, block_mode_);
    ASSERT_NE(AUTH_REQUIRED_RESPONSE_IO_PENDING, auth_retval);
    auth_retval_ = auth_retval;
  }
  void set_auth_credentials(const AuthCredentials& auth_credentials) {
    auth_credentials_ = auth_credentials;
  }

  void set_redirect_url(const GURL& url) {
    redirect_url_ = url;
  }

  void set_block_on(int block_on) {
    block_on_ = block_on;
  }

  // Allows the user to check in which state did we block.
  Stage stage_blocked_for_callback() const {
    EXPECT_EQ(USER_CALLBACK, block_mode_);
    return stage_blocked_for_callback_;
  }

 private:
  void RunCallback(int response, const CompletionCallback& callback);
  void RunAuthCallback(AuthRequiredResponse response,
                       const AuthCallback& callback);

  // TestNetworkDelegate implementation.
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 const CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE;

  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) OVERRIDE;

  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE;

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) OVERRIDE;

  // Resets the callbacks and |stage_blocked_for_callback_|.
  void Reset();

  // Checks whether we should block in |stage|. If yes, returns an error code
  // and optionally sets up callback based on |block_mode_|. If no, returns OK.
  int MaybeBlockStage(Stage stage, const CompletionCallback& callback);

  // Configuration parameters, can be adjusted by public methods:
  const BlockMode block_mode_;

  // Values returned on blocking stages when mode is SYNCHRONOUS or
  // AUTO_CALLBACK. For USER_CALLBACK these are set automatically to IO_PENDING.
  int retval_;  // To be returned in non-auth stages.
  AuthRequiredResponse auth_retval_;

  GURL redirect_url_;  // Used if non-empty during OnBeforeURLRequest.
  int block_on_;  // Bit mask: in which stages to block.

  // |auth_credentials_| will be copied to |*target_auth_credential_| on
  // callback.
  AuthCredentials auth_credentials_;
  AuthCredentials* target_auth_credentials_;

  // Internal variables, not set by not the user:
  // Last blocked stage waiting for user callback (unused if |block_mode_| !=
  // USER_CALLBACK).
  Stage stage_blocked_for_callback_;

  // Callback objects stored during blocking stages.
  CompletionCallback callback_;
  AuthCallback auth_callback_;

  base::WeakPtrFactory<BlockingNetworkDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlockingNetworkDelegate);
};

BlockingNetworkDelegate::BlockingNetworkDelegate(BlockMode block_mode)
    : block_mode_(block_mode),
      retval_(OK),
      auth_retval_(AUTH_REQUIRED_RESPONSE_NO_ACTION),
      block_on_(0),
      target_auth_credentials_(NULL),
      stage_blocked_for_callback_(NOT_BLOCKED),
      weak_factory_(this) {
}

void BlockingNetworkDelegate::DoCallback(int response) {
  ASSERT_EQ(USER_CALLBACK, block_mode_);
  ASSERT_NE(NOT_BLOCKED, stage_blocked_for_callback_);
  ASSERT_NE(ON_AUTH_REQUIRED, stage_blocked_for_callback_);
  CompletionCallback callback = callback_;
  Reset();
  RunCallback(response, callback);
}

void BlockingNetworkDelegate::DoAuthCallback(
    NetworkDelegate::AuthRequiredResponse response) {
  ASSERT_EQ(USER_CALLBACK, block_mode_);
  ASSERT_EQ(ON_AUTH_REQUIRED, stage_blocked_for_callback_);
  AuthCallback auth_callback = auth_callback_;
  Reset();
  RunAuthCallback(response, auth_callback);
}

void BlockingNetworkDelegate::RunCallback(int response,
                                          const CompletionCallback& callback) {
  callback.Run(response);
}

void BlockingNetworkDelegate::RunAuthCallback(AuthRequiredResponse response,
                                              const AuthCallback& callback) {
  if (auth_retval_ == AUTH_REQUIRED_RESPONSE_SET_AUTH) {
    ASSERT_TRUE(target_auth_credentials_ != NULL);
    *target_auth_credentials_ = auth_credentials_;
  }
  callback.Run(response);
}

int BlockingNetworkDelegate::OnBeforeURLRequest(
    URLRequest* request,
    const CompletionCallback& callback,
    GURL* new_url) {
  if (redirect_url_ == request->url())
    return OK;  // We've already seen this request and redirected elsewhere.

  TestNetworkDelegate::OnBeforeURLRequest(request, callback, new_url);

  if (!redirect_url_.is_empty())
    *new_url = redirect_url_;

  return MaybeBlockStage(ON_BEFORE_URL_REQUEST, callback);
}

int BlockingNetworkDelegate::OnBeforeSendHeaders(
    URLRequest* request,
    const CompletionCallback& callback,
    HttpRequestHeaders* headers) {
  TestNetworkDelegate::OnBeforeSendHeaders(request, callback, headers);

  return MaybeBlockStage(ON_BEFORE_SEND_HEADERS, callback);
}

int BlockingNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  TestNetworkDelegate::OnHeadersReceived(request,
                                         callback,
                                         original_response_headers,
                                         override_response_headers,
                                         allowed_unsafe_redirect_url);

  return MaybeBlockStage(ON_HEADERS_RECEIVED, callback);
}

NetworkDelegate::AuthRequiredResponse BlockingNetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  TestNetworkDelegate::OnAuthRequired(request, auth_info, callback,
                                      credentials);
  // Check that the user has provided callback for the previous blocked stage.
  EXPECT_EQ(NOT_BLOCKED, stage_blocked_for_callback_);

  if ((block_on_ & ON_AUTH_REQUIRED) == 0) {
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  target_auth_credentials_ = credentials;

  switch (block_mode_) {
    case SYNCHRONOUS:
      if (auth_retval_ == AUTH_REQUIRED_RESPONSE_SET_AUTH)
        *target_auth_credentials_ = auth_credentials_;
      return auth_retval_;

    case AUTO_CALLBACK:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&BlockingNetworkDelegate::RunAuthCallback,
                     weak_factory_.GetWeakPtr(), auth_retval_, callback));
      return AUTH_REQUIRED_RESPONSE_IO_PENDING;

    case USER_CALLBACK:
      auth_callback_ = callback;
      stage_blocked_for_callback_ = ON_AUTH_REQUIRED;
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
      return AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  NOTREACHED();
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;  // Dummy value.
}

void BlockingNetworkDelegate::Reset() {
  EXPECT_NE(NOT_BLOCKED, stage_blocked_for_callback_);
  stage_blocked_for_callback_ = NOT_BLOCKED;
  callback_.Reset();
  auth_callback_.Reset();
}

int BlockingNetworkDelegate::MaybeBlockStage(
    BlockingNetworkDelegate::Stage stage,
    const CompletionCallback& callback) {
  // Check that the user has provided callback for the previous blocked stage.
  EXPECT_EQ(NOT_BLOCKED, stage_blocked_for_callback_);

  if ((block_on_ & stage) == 0) {
    return OK;
  }

  switch (block_mode_) {
    case SYNCHRONOUS:
      EXPECT_NE(OK, retval_);
      return retval_;

    case AUTO_CALLBACK:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&BlockingNetworkDelegate::RunCallback,
                     weak_factory_.GetWeakPtr(), retval_, callback));
      return ERR_IO_PENDING;

    case USER_CALLBACK:
      callback_ = callback;
      stage_blocked_for_callback_ = stage;
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
      return ERR_IO_PENDING;
  }
  NOTREACHED();
  return 0;
}

class TestURLRequestContextWithProxy : public TestURLRequestContext {
 public:
  // Does not own |delegate|.
  TestURLRequestContextWithProxy(const std::string& proxy,
                                 NetworkDelegate* delegate)
      : TestURLRequestContext(true) {
    context_storage_.set_proxy_service(ProxyService::CreateFixed(proxy));
    set_network_delegate(delegate);
    Init();
  }
  virtual ~TestURLRequestContextWithProxy() {}
};

}  // namespace

// Inherit PlatformTest since we require the autorelease pool on Mac OS X.
class URLRequestTest : public PlatformTest {
 public:
  URLRequestTest() : default_context_(true) {
    default_context_.set_network_delegate(&default_network_delegate_);
    default_context_.set_net_log(&net_log_);
    job_factory_.SetProtocolHandler("data", new DataProtocolHandler);
#if !defined(DISABLE_FILE_SUPPORT)
    job_factory_.SetProtocolHandler(
        "file", new FileProtocolHandler(base::MessageLoopProxy::current()));
#endif
    default_context_.set_job_factory(&job_factory_);
    default_context_.Init();
  }
  virtual ~URLRequestTest() {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  // Adds the TestJobInterceptor to the default context.
  TestJobInterceptor* AddTestInterceptor() {
    TestJobInterceptor* protocol_handler_ = new TestJobInterceptor();
    job_factory_.SetProtocolHandler("http", NULL);
    job_factory_.SetProtocolHandler("http", protocol_handler_);
    return protocol_handler_;
  }

 protected:
  CapturingNetLog net_log_;
  TestNetworkDelegate default_network_delegate_;  // Must outlive URLRequest.
  URLRequestJobFactoryImpl job_factory_;
  TestURLRequestContext default_context_;
};

TEST_F(URLRequestTest, AboutBlankTest) {
  TestDelegate d;
  {
    URLRequest r(GURL("about:blank"), DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_TRUE(!r.is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
    EXPECT_EQ("", r.GetSocketAddress().host());
    EXPECT_EQ(0, r.GetSocketAddress().port());

    HttpRequestHeaders headers;
    EXPECT_FALSE(r.GetFullRequestHeaders(&headers));
  }
}

TEST_F(URLRequestTest, DataURLImageTest) {
  TestDelegate d;
  {
    // Use our nice little Chrome logo.
    URLRequest r(
        GURL(
        "data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAADVklEQVQ4jX2TfUwUBBjG3"
        "w1y+HGcd9dxhXR8T4awOccJGgOSWclHImznLkTlSw0DDQXkrmgYgbUYnlQTqQxIEVxitD"
        "5UMCATRA1CEEg+Qjw3bWDxIauJv/5oumqs39/P827vnucRmYN0gyF01GI5MpCVdW0gO7t"
        "vNC+vqSEtbZefk5NuLv1jdJ46p/zw0HeH4+PHr3h7c1mjoV2t5rKzMx1+fg9bAgK6zHq9"
        "cU5z+LpA3xOtx34+vTeT21onRuzssC3zxbbSwC13d/pFuC7CkIMDxQpF7r/MWq12UctI1"
        "dWWm99ypqSYmRUBdKem8MkrO/kgaTt1O7YzlpzE5GIVd0WYUqt57yWf2McHTObYPbVD+Z"
        "wbtlLTVMZ3BW+TnLyXLaWtmEq6WJVbT3HBh3Svj2HQQcm43XwmtoYM6vVKleh0uoWvnzW"
        "3v3MpidruPTQPf0bia7sJOtBM0ufTWNvus/nkDFHF9ZS+uYVjRUasMeHUmyLYtcklTvzW"
        "GFZnNOXczThvpKIzjcahSqIzkvDLayDq6D3eOjtBbNUEIZYyqsvj4V4wY92eNJ4IoyhTb"
        "xXX1T5xsV9tm9r4TQwHLiZw/pdDZJea8TKmsmR/K0uLh/GwnCHghTja6lPhphezPfO5/5"
        "MrVvMzNaI3+ERHfrFzPKQukrQGI4d/3EFD/3E2mVNYvi4at7CXWREaxZGD+3hg28zD3gV"
        "Md6q5c8GdosynKmSeRuGzpjyl1/9UDGtPR5HeaKT8Wjo17WXk579BXVUhN64ehF9fhRtq"
        "/uxxZKzNiZFGD0wRC3NFROZ5mwIPL/96K/rKMMLrIzF9uhHr+/sYH7DAbwlgC4J+R2Z7F"
        "Ux1qLnV7MGF40smVSoJ/jvHRfYhQeUJd/SnYtGWhPHR0Sz+GE2F2yth0B36Vcz2KpnufB"
        "JbsysjjW4kblBUiIjiURUWqJY65zxbnTy57GQyH58zgy0QBtTQv5gH15XMdKkYu+TGaJM"
        "nlm2O34uI4b9tflqp1+QEFGzoW/ulmcofcpkZCYJhDfSpme7QcrHa+Xfji8paEQkTkSfm"
        "moRWRNZr/F1KfVMjW+IKEnv2FwZfKdzt0BQR6lClcZR0EfEXEfv/G6W9iLiIyCoReV5En"
        "hORIBHx+ufPj/gLB/zGI/G4Bk0AAAAASUVORK5CYII="),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_TRUE(!r.is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 911);
    EXPECT_EQ("", r.GetSocketAddress().host());
    EXPECT_EQ(0, r.GetSocketAddress().port());

    HttpRequestHeaders headers;
    EXPECT_FALSE(r.GetFullRequestHeaders(&headers));
  }
}

#if !defined(DISABLE_FILE_SUPPORT)
TEST_F(URLRequestTest, FileTest) {
  base::FilePath app_path;
  PathService::Get(base::FILE_EXE, &app_path);
  GURL app_url = FilePathToFileURL(app_path);

  TestDelegate d;
  {
    URLRequest r(app_url, DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = -1;
    EXPECT_TRUE(base::GetFileSize(app_path, &file_size));

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
    EXPECT_EQ("", r.GetSocketAddress().host());
    EXPECT_EQ(0, r.GetSocketAddress().port());

    HttpRequestHeaders headers;
    EXPECT_FALSE(r.GetFullRequestHeaders(&headers));
  }
}

TEST_F(URLRequestTest, FileTestCancel) {
  base::FilePath app_path;
  PathService::Get(base::FILE_EXE, &app_path);
  GURL app_url = FilePathToFileURL(app_path);

  TestDelegate d;
  {
    URLRequest r(app_url, DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());
    r.Cancel();
  }
  // Async cancellation should be safe even when URLRequest has been already
  // destroyed.
  base::RunLoop().RunUntilIdle();
}

TEST_F(URLRequestTest, FileTestFullSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  GURL temp_url = FilePathToFileURL(temp_path);
  EXPECT_TRUE(base::WriteFile(temp_path, buffer.get(), buffer_size));

  int64 file_size;
  EXPECT_TRUE(base::GetFileSize(temp_path, &file_size));

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  const size_t content_length = last_byte_position - first_byte_position + 1;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  TestDelegate d;
  {
    URLRequest r(temp_url, DEFAULT_PRIORITY, &d, &default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(
        HttpRequestHeaders::kRange,
        net::HttpByteRange::Bounded(
            first_byte_position, last_byte_position).GetHeaderValue());
    r.SetExtraRequestHeaders(headers);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();
    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(static_cast<int>(content_length), d.bytes_received());
    // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
    EXPECT_TRUE(partial_buffer_string == d.data_received());
  }

  EXPECT_TRUE(base::DeleteFile(temp_path, false));
}

TEST_F(URLRequestTest, FileTestHalfSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  GURL temp_url = FilePathToFileURL(temp_path);
  EXPECT_TRUE(base::WriteFile(temp_path, buffer.get(), buffer_size));

  int64 file_size;
  EXPECT_TRUE(base::GetFileSize(temp_path, &file_size));

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - 1;
  const size_t content_length = last_byte_position - first_byte_position + 1;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  TestDelegate d;
  {
    URLRequest r(temp_url, DEFAULT_PRIORITY, &d, &default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange,
                      net::HttpByteRange::RightUnbounded(
                          first_byte_position).GetHeaderValue());
    r.SetExtraRequestHeaders(headers);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();
    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(static_cast<int>(content_length), d.bytes_received());
    // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
    EXPECT_TRUE(partial_buffer_string == d.data_received());
  }

  EXPECT_TRUE(base::DeleteFile(temp_path, false));
}

TEST_F(URLRequestTest, FileTestMultipleRanges) {
  const size_t buffer_size = 400000;
  scoped_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  GURL temp_url = FilePathToFileURL(temp_path);
  EXPECT_TRUE(base::WriteFile(temp_path, buffer.get(), buffer_size));

  int64 file_size;
  EXPECT_TRUE(base::GetFileSize(temp_path, &file_size));

  TestDelegate d;
  {
    URLRequest r(temp_url, DEFAULT_PRIORITY, &d, &default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange, "bytes=0-0,10-200,200-300");
    r.SetExtraRequestHeaders(headers);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();
    EXPECT_TRUE(d.request_failed());
  }

  EXPECT_TRUE(base::DeleteFile(temp_path, false));
}

TEST_F(URLRequestTest, AllowFileURLs) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &test_file));
  std::string test_data("monkey");
  base::WriteFile(test_file, test_data.data(), test_data.size());
  GURL test_file_url = net::FilePathToFileURL(test_file);

  {
    TestDelegate d;
    TestNetworkDelegate network_delegate;
    network_delegate.set_can_access_files(true);
    default_context_.set_network_delegate(&network_delegate);
    URLRequest r(test_file_url, DEFAULT_PRIORITY, &d, &default_context_);
    r.Start();
    base::RunLoop().Run();
    EXPECT_FALSE(d.request_failed());
    EXPECT_EQ(test_data, d.data_received());
  }

  {
    TestDelegate d;
    TestNetworkDelegate network_delegate;
    network_delegate.set_can_access_files(false);
    default_context_.set_network_delegate(&network_delegate);
    URLRequest r(test_file_url, DEFAULT_PRIORITY, &d, &default_context_);
    r.Start();
    base::RunLoop().Run();
    EXPECT_TRUE(d.request_failed());
    EXPECT_EQ("", d.data_received());
  }
}


TEST_F(URLRequestTest, FileDirCancelTest) {
  // Put in mock resource provider.
  NetModule::SetResourceProvider(TestNetResourceProvider);

  TestDelegate d;
  {
    base::FilePath file_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("net"));
    file_path = file_path.Append(FILE_PATH_LITERAL("data"));

    URLRequest req(
        FilePathToFileURL(file_path), DEFAULT_PRIORITY, &d, &default_context_);
    req.Start();
    EXPECT_TRUE(req.is_pending());

    d.set_cancel_in_received_data_pending(true);

    base::RunLoop().Run();
  }

  // Take out mock resource provider.
  NetModule::SetResourceProvider(NULL);
}

TEST_F(URLRequestTest, FileDirOutputSanity) {
  // Verify the general sanity of the the output of the file:
  // directory lister by checking for the output of a known existing
  // file.
  const char sentinel_name[] = "filedir-sentinel";

  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("net"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));

  TestDelegate d;
  URLRequest req(
      FilePathToFileURL(path), DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();

  // Generate entry for the sentinel file.
  base::FilePath sentinel_path = path.AppendASCII(sentinel_name);
  base::File::Info info;
  EXPECT_TRUE(base::GetFileInfo(sentinel_path, &info));
  EXPECT_GT(info.size, 0);
  std::string sentinel_output = GetDirectoryListingEntry(
      base::string16(sentinel_name, sentinel_name + strlen(sentinel_name)),
      std::string(sentinel_name),
      false /* is_dir */,
      info.size,
      info.last_modified);

  ASSERT_LT(0, d.bytes_received());
  ASSERT_FALSE(d.request_failed());
  ASSERT_TRUE(req.status().is_success());
  // Check for the entry generated for the "sentinel" file.
  const std::string& data = d.data_received();
  ASSERT_NE(data.find(sentinel_output), std::string::npos);
}

TEST_F(URLRequestTest, FileDirRedirectNoCrash) {
  // There is an implicit redirect when loading a file path that matches a
  // directory and does not end with a slash.  Ensure that following such
  // redirects does not crash.  See http://crbug.com/18686.

  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("net"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));

  TestDelegate d;
  URLRequest req(
      FilePathToFileURL(path), DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();

  ASSERT_EQ(1, d.received_redirect_count());
  ASSERT_LT(0, d.bytes_received());
  ASSERT_FALSE(d.request_failed());
  ASSERT_TRUE(req.status().is_success());
}

#if defined(OS_WIN)
// Don't accept the url "file:///" on windows. See http://crbug.com/1474.
TEST_F(URLRequestTest, FileDirRedirectSingleSlash) {
  TestDelegate d;
  URLRequest req(GURL("file:///"), DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();

  ASSERT_EQ(1, d.received_redirect_count());
  ASSERT_FALSE(req.status().is_success());
}
#endif  // defined(OS_WIN)

#endif  // !defined(DISABLE_FILE_SUPPORT)

TEST_F(URLRequestTest, InvalidUrlTest) {
  TestDelegate d;
  {
    URLRequest r(GURL("invalid url"), DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();
    EXPECT_TRUE(d.request_failed());
  }
}

#if defined(OS_WIN)
TEST_F(URLRequestTest, ResolveShortcutTest) {
  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("net");
  app_path = app_path.AppendASCII("data");
  app_path = app_path.AppendASCII("url_request_unittest");
  app_path = app_path.AppendASCII("with-headers.html");

  std::wstring lnk_path = app_path.value() + L".lnk";

  base::win::ScopedCOMInitializer com_initializer;

  // Temporarily create a shortcut for test
  {
    base::win::ScopedComPtr<IShellLink> shell;
    ASSERT_TRUE(SUCCEEDED(shell.CreateInstance(CLSID_ShellLink, NULL,
                                               CLSCTX_INPROC_SERVER)));
    base::win::ScopedComPtr<IPersistFile> persist;
    ASSERT_TRUE(SUCCEEDED(shell.QueryInterface(persist.Receive())));
    EXPECT_TRUE(SUCCEEDED(shell->SetPath(app_path.value().c_str())));
    EXPECT_TRUE(SUCCEEDED(shell->SetDescription(L"ResolveShortcutTest")));
    EXPECT_TRUE(SUCCEEDED(persist->Save(lnk_path.c_str(), TRUE)));
  }

  TestDelegate d;
  {
    URLRequest r(FilePathToFileURL(base::FilePath(lnk_path)),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    WIN32_FILE_ATTRIBUTE_DATA data;
    GetFileAttributesEx(app_path.value().c_str(),
                        GetFileExInfoStandard, &data);
    HANDLE file = CreateFile(app_path.value().c_str(), GENERIC_READ,
                             FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT_NE(INVALID_HANDLE_VALUE, file);
    scoped_ptr<char[]> buffer(new char[data.nFileSizeLow]);
    DWORD read_size;
    BOOL result;
    result = ReadFile(file, buffer.get(), data.nFileSizeLow,
                      &read_size, NULL);
    std::string content(buffer.get(), read_size);
    CloseHandle(file);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(content, d.data_received());
  }

  // Clean the shortcut
  DeleteFile(lnk_path.c_str());
}
#endif  // defined(OS_WIN)

// Custom URLRequestJobs for use with interceptor tests
class RestartTestJob : public URLRequestTestJob {
 public:
  RestartTestJob(URLRequest* request, NetworkDelegate* network_delegate)
    : URLRequestTestJob(request, network_delegate, true) {}
 protected:
  virtual void StartAsync() OVERRIDE {
    this->NotifyRestartRequired();
  }
 private:
  virtual ~RestartTestJob() {}
};

class CancelTestJob : public URLRequestTestJob {
 public:
  explicit CancelTestJob(URLRequest* request, NetworkDelegate* network_delegate)
    : URLRequestTestJob(request, network_delegate, true) {}
 protected:
  virtual void StartAsync() OVERRIDE {
    request_->Cancel();
  }
 private:
  virtual ~CancelTestJob() {}
};

class CancelThenRestartTestJob : public URLRequestTestJob {
 public:
  explicit CancelThenRestartTestJob(URLRequest* request,
                                    NetworkDelegate* network_delegate)
      : URLRequestTestJob(request, network_delegate, true) {
  }
 protected:
  virtual void StartAsync() OVERRIDE {
    request_->Cancel();
    this->NotifyRestartRequired();
  }
 private:
  virtual ~CancelThenRestartTestJob() {}
};

// An Interceptor for use with interceptor tests
class TestInterceptor : URLRequest::Interceptor {
 public:
  TestInterceptor()
      : intercept_main_request_(false), restart_main_request_(false),
        cancel_main_request_(false), cancel_then_restart_main_request_(false),
        simulate_main_network_error_(false),
        intercept_redirect_(false), cancel_redirect_request_(false),
        intercept_final_response_(false), cancel_final_request_(false),
        did_intercept_main_(false), did_restart_main_(false),
        did_cancel_main_(false), did_cancel_then_restart_main_(false),
        did_simulate_error_main_(false),
        did_intercept_redirect_(false), did_cancel_redirect_(false),
        did_intercept_final_(false), did_cancel_final_(false) {
    URLRequest::Deprecated::RegisterRequestInterceptor(this);
  }

  virtual ~TestInterceptor() {
    URLRequest::Deprecated::UnregisterRequestInterceptor(this);
  }

  virtual URLRequestJob* MaybeIntercept(
      URLRequest* request,
      NetworkDelegate* network_delegate) OVERRIDE {
    if (restart_main_request_) {
      restart_main_request_ = false;
      did_restart_main_ = true;
      return new RestartTestJob(request, network_delegate);
    }
    if (cancel_main_request_) {
      cancel_main_request_ = false;
      did_cancel_main_ = true;
      return new CancelTestJob(request, network_delegate);
    }
    if (cancel_then_restart_main_request_) {
      cancel_then_restart_main_request_ = false;
      did_cancel_then_restart_main_ = true;
      return new CancelThenRestartTestJob(request, network_delegate);
    }
    if (simulate_main_network_error_) {
      simulate_main_network_error_ = false;
      did_simulate_error_main_ = true;
      // will error since the requeted url is not one of its canned urls
      return new URLRequestTestJob(request, network_delegate, true);
    }
    if (!intercept_main_request_)
      return NULL;
    intercept_main_request_ = false;
    did_intercept_main_ = true;
    URLRequestTestJob* job =  new URLRequestTestJob(request,
                                                    network_delegate,
                                                    main_headers_,
                                                    main_data_,
                                                    true);
    job->set_load_timing_info(main_request_load_timing_info_);
    return job;
  }

  virtual URLRequestJob* MaybeInterceptRedirect(
      URLRequest* request,
      NetworkDelegate* network_delegate,
      const GURL& location) OVERRIDE {
    if (cancel_redirect_request_) {
      cancel_redirect_request_ = false;
      did_cancel_redirect_ = true;
      return new CancelTestJob(request, network_delegate);
    }
    if (!intercept_redirect_)
      return NULL;
    intercept_redirect_ = false;
    did_intercept_redirect_ = true;
    return new URLRequestTestJob(request,
                                 network_delegate,
                                 redirect_headers_,
                                 redirect_data_,
                                 true);
  }

  virtual URLRequestJob* MaybeInterceptResponse(
      URLRequest* request, NetworkDelegate* network_delegate) OVERRIDE {
    if (cancel_final_request_) {
      cancel_final_request_ = false;
      did_cancel_final_ = true;
      return new CancelTestJob(request, network_delegate);
    }
    if (!intercept_final_response_)
      return NULL;
    intercept_final_response_ = false;
    did_intercept_final_ = true;
    return new URLRequestTestJob(request,
                                 network_delegate,
                                 final_headers_,
                                 final_data_,
                                 true);
  }

  // Whether to intercept the main request, and if so the response to return and
  // the LoadTimingInfo to use.
  bool intercept_main_request_;
  std::string main_headers_;
  std::string main_data_;
  LoadTimingInfo main_request_load_timing_info_;

  // Other actions we take at MaybeIntercept time
  bool restart_main_request_;
  bool cancel_main_request_;
  bool cancel_then_restart_main_request_;
  bool simulate_main_network_error_;

  // Whether to intercept redirects, and if so the response to return.
  bool intercept_redirect_;
  std::string redirect_headers_;
  std::string redirect_data_;

  // Other actions we can take at MaybeInterceptRedirect time
  bool cancel_redirect_request_;

  // Whether to intercept final response, and if so the response to return.
  bool intercept_final_response_;
  std::string final_headers_;
  std::string final_data_;

  // Other actions we can take at MaybeInterceptResponse time
  bool cancel_final_request_;

  // If we did something or not
  bool did_intercept_main_;
  bool did_restart_main_;
  bool did_cancel_main_;
  bool did_cancel_then_restart_main_;
  bool did_simulate_error_main_;
  bool did_intercept_redirect_;
  bool did_cancel_redirect_;
  bool did_intercept_final_;
  bool did_cancel_final_;

  // Static getters for canned response header and data strings

  static std::string ok_data() {
    return URLRequestTestJob::test_data_1();
  }

  static std::string ok_headers() {
    return URLRequestTestJob::test_headers();
  }

  static std::string redirect_data() {
    return std::string();
  }

  static std::string redirect_headers() {
    return URLRequestTestJob::test_redirect_headers();
  }

  static std::string error_data() {
    return std::string("ohhh nooooo mr. bill!");
  }

  static std::string error_headers() {
    return URLRequestTestJob::test_error_headers();
  }
};

TEST_F(URLRequestTest, Intercept) {
  TestInterceptor interceptor;

  // intercept the main request and respond with a simple response
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::ok_headers();
  interceptor.main_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  base::SupportsUserData::Data* user_data0 = new base::SupportsUserData::Data();
  base::SupportsUserData::Data* user_data1 = new base::SupportsUserData::Data();
  base::SupportsUserData::Data* user_data2 = new base::SupportsUserData::Data();
  req.SetUserData(NULL, user_data0);
  req.SetUserData(&user_data1, user_data1);
  req.SetUserData(&user_data2, user_data2);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Make sure we can retrieve our specific user data
  EXPECT_EQ(user_data0, req.GetUserData(NULL));
  EXPECT_EQ(user_data1, req.GetUserData(&user_data1));
  EXPECT_EQ(user_data2, req.GetUserData(&user_data2));

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);

  // Check we got one good response
  EXPECT_TRUE(req.status().is_success());
  EXPECT_EQ(200, req.response_headers()->response_code());
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptRedirect) {
  TestInterceptor interceptor;

  // intercept the main request and respond with a redirect
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::redirect_headers();
  interceptor.main_data_ = TestInterceptor::redirect_data();

  // intercept that redirect and respond a final OK response
  interceptor.intercept_redirect_ = true;
  interceptor.redirect_headers_ =  TestInterceptor::ok_headers();
  interceptor.redirect_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);
  EXPECT_TRUE(interceptor.did_intercept_redirect_);

  // Check we got one good response
  EXPECT_TRUE(req.status().is_success());
  if (req.status().is_success()) {
    EXPECT_EQ(200, req.response_headers()->response_code());
  }
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptServerError) {
  TestInterceptor interceptor;

  // intercept the main request to generate a server error response
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::error_headers();
  interceptor.main_data_ = TestInterceptor::error_data();

  // intercept that error and respond with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);
  EXPECT_TRUE(interceptor.did_intercept_final_);

  // Check we got one good response
  EXPECT_TRUE(req.status().is_success());
  EXPECT_EQ(200, req.response_headers()->response_code());
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptNetworkError) {
  TestInterceptor interceptor;

  // intercept the main request to simulate a network error
  interceptor.simulate_main_network_error_ = true;

  // intercept that error and respond with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_simulate_error_main_);
  EXPECT_TRUE(interceptor.did_intercept_final_);

  // Check we received one good response
  EXPECT_TRUE(req.status().is_success());
  EXPECT_EQ(200, req.response_headers()->response_code());
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptRestartRequired) {
  TestInterceptor interceptor;

  // restart the main request
  interceptor.restart_main_request_ = true;

  // then intercept the new main request and respond with an OK response
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::ok_headers();
  interceptor.main_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_restart_main_);
  EXPECT_TRUE(interceptor.did_intercept_main_);

  // Check we received one good response
  EXPECT_TRUE(req.status().is_success());
  if (req.status().is_success()) {
    EXPECT_EQ(200, req.response_headers()->response_code());
  }
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptRespectsCancelMain) {
  TestInterceptor interceptor;

  // intercept the main request and cancel from within the restarted job
  interceptor.cancel_main_request_ = true;

  // setup to intercept final response and override it with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_cancel_main_);
  EXPECT_FALSE(interceptor.did_intercept_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

TEST_F(URLRequestTest, InterceptRespectsCancelRedirect) {
  TestInterceptor interceptor;

  // intercept the main request and respond with a redirect
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::redirect_headers();
  interceptor.main_data_ = TestInterceptor::redirect_data();

  // intercept the redirect and cancel from within that job
  interceptor.cancel_redirect_request_ = true;

  // setup to intercept final response and override it with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);
  EXPECT_TRUE(interceptor.did_cancel_redirect_);
  EXPECT_FALSE(interceptor.did_intercept_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

TEST_F(URLRequestTest, InterceptRespectsCancelFinal) {
  TestInterceptor interceptor;

  // intercept the main request to simulate a network error
  interceptor.simulate_main_network_error_ = true;

  // setup to intercept final response and cancel from within that job
  interceptor.cancel_final_request_ = true;

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_simulate_error_main_);
  EXPECT_TRUE(interceptor.did_cancel_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

TEST_F(URLRequestTest, InterceptRespectsCancelInRestart) {
  TestInterceptor interceptor;

  // intercept the main request and cancel then restart from within that job
  interceptor.cancel_then_restart_main_request_ = true;

  // setup to intercept final response and override it with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("GET");
  req.Start();
  base::RunLoop().Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_cancel_then_restart_main_);
  EXPECT_FALSE(interceptor.did_intercept_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

LoadTimingInfo RunLoadTimingTest(const LoadTimingInfo& job_load_timing,
                                 URLRequestContext* context) {
  TestInterceptor interceptor;
  interceptor.intercept_main_request_ = true;
  interceptor.main_request_load_timing_info_ = job_load_timing;
  TestDelegate d;
  URLRequest req(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d, context);
  req.Start();
  base::RunLoop().Run();

  LoadTimingInfo resulting_load_timing;
  req.GetLoadTimingInfo(&resulting_load_timing);

  // None of these should be modified by the URLRequest.
  EXPECT_EQ(job_load_timing.socket_reused, resulting_load_timing.socket_reused);
  EXPECT_EQ(job_load_timing.socket_log_id, resulting_load_timing.socket_log_id);
  EXPECT_EQ(job_load_timing.send_start, resulting_load_timing.send_start);
  EXPECT_EQ(job_load_timing.send_end, resulting_load_timing.send_end);
  EXPECT_EQ(job_load_timing.receive_headers_end,
            resulting_load_timing.receive_headers_end);

  return resulting_load_timing;
}

// "Normal" LoadTimingInfo as returned by a job.  Everything is in order, not
// reused.  |connect_time_flags| is used to indicate if there should be dns
// or SSL times, and |used_proxy| is used for proxy times.
LoadTimingInfo NormalLoadTimingInfo(base::TimeTicks now,
                                    int connect_time_flags,
                                    bool used_proxy) {
  LoadTimingInfo load_timing;
  load_timing.socket_log_id = 1;

  if (used_proxy) {
    load_timing.proxy_resolve_start = now + base::TimeDelta::FromDays(1);
    load_timing.proxy_resolve_end = now + base::TimeDelta::FromDays(2);
  }

  LoadTimingInfo::ConnectTiming& connect_timing = load_timing.connect_timing;
  if (connect_time_flags & CONNECT_TIMING_HAS_DNS_TIMES) {
    connect_timing.dns_start = now + base::TimeDelta::FromDays(3);
    connect_timing.dns_end = now + base::TimeDelta::FromDays(4);
  }
  connect_timing.connect_start = now + base::TimeDelta::FromDays(5);
  if (connect_time_flags & CONNECT_TIMING_HAS_SSL_TIMES) {
    connect_timing.ssl_start = now + base::TimeDelta::FromDays(6);
    connect_timing.ssl_end = now + base::TimeDelta::FromDays(7);
  }
  connect_timing.connect_end = now + base::TimeDelta::FromDays(8);

  load_timing.send_start = now + base::TimeDelta::FromDays(9);
  load_timing.send_end = now + base::TimeDelta::FromDays(10);
  load_timing.receive_headers_end = now + base::TimeDelta::FromDays(11);
  return load_timing;
}

// Same as above, but in the case of a reused socket.
LoadTimingInfo NormalLoadTimingInfoReused(base::TimeTicks now,
                                          bool used_proxy) {
  LoadTimingInfo load_timing;
  load_timing.socket_log_id = 1;
  load_timing.socket_reused = true;

  if (used_proxy) {
    load_timing.proxy_resolve_start = now + base::TimeDelta::FromDays(1);
    load_timing.proxy_resolve_end = now + base::TimeDelta::FromDays(2);
  }

  load_timing.send_start = now + base::TimeDelta::FromDays(9);
  load_timing.send_end = now + base::TimeDelta::FromDays(10);
  load_timing.receive_headers_end = now + base::TimeDelta::FromDays(11);
  return load_timing;
}

// Basic test that the intercept + load timing tests work.
TEST_F(URLRequestTest, InterceptLoadTiming) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_DNS_TIMES, false);

  LoadTimingInfo load_timing_result =
      RunLoadTimingTest(job_load_timing, &default_context_);

  // Nothing should have been changed by the URLRequest.
  EXPECT_EQ(job_load_timing.proxy_resolve_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(job_load_timing.proxy_resolve_end,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(job_load_timing.connect_timing.dns_start,
            load_timing_result.connect_timing.dns_start);
  EXPECT_EQ(job_load_timing.connect_timing.dns_end,
            load_timing_result.connect_timing.dns_end);
  EXPECT_EQ(job_load_timing.connect_timing.connect_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(job_load_timing.connect_timing.connect_end,
            load_timing_result.connect_timing.connect_end);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_start,
            load_timing_result.connect_timing.ssl_start);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_end,
            load_timing_result.connect_timing.ssl_end);

  // Redundant sanity check.
  TestLoadTimingNotReused(load_timing_result, CONNECT_TIMING_HAS_DNS_TIMES);
}

// Another basic test, with proxy and SSL times, but no DNS times.
TEST_F(URLRequestTest, InterceptLoadTimingProxy) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_SSL_TIMES, true);

  LoadTimingInfo load_timing_result =
      RunLoadTimingTest(job_load_timing, &default_context_);

  // Nothing should have been changed by the URLRequest.
  EXPECT_EQ(job_load_timing.proxy_resolve_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(job_load_timing.proxy_resolve_end,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(job_load_timing.connect_timing.dns_start,
            load_timing_result.connect_timing.dns_start);
  EXPECT_EQ(job_load_timing.connect_timing.dns_end,
            load_timing_result.connect_timing.dns_end);
  EXPECT_EQ(job_load_timing.connect_timing.connect_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(job_load_timing.connect_timing.connect_end,
            load_timing_result.connect_timing.connect_end);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_start,
            load_timing_result.connect_timing.ssl_start);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_end,
            load_timing_result.connect_timing.ssl_end);

  // Redundant sanity check.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_SSL_TIMES);
}

// Make sure that URLRequest correctly adjusts proxy times when they're before
// |request_start|, due to already having a connected socket.  This happens in
// the case of reusing a SPDY session.  The connected socket is not considered
// reused in this test (May be a preconnect).
//
// To mix things up from the test above, assumes DNS times but no SSL times.
TEST_F(URLRequestTest, InterceptLoadTimingEarlyProxyResolution) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_DNS_TIMES, true);
  job_load_timing.proxy_resolve_start = now - base::TimeDelta::FromDays(6);
  job_load_timing.proxy_resolve_end = now - base::TimeDelta::FromDays(5);
  job_load_timing.connect_timing.dns_start = now - base::TimeDelta::FromDays(4);
  job_load_timing.connect_timing.dns_end = now - base::TimeDelta::FromDays(3);
  job_load_timing.connect_timing.connect_start =
      now - base::TimeDelta::FromDays(2);
  job_load_timing.connect_timing.connect_end =
      now - base::TimeDelta::FromDays(1);

  LoadTimingInfo load_timing_result =
      RunLoadTimingTest(job_load_timing, &default_context_);

  // Proxy times, connect times, and DNS times should all be replaced with
  // request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.dns_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.dns_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_DNS_TIMES);
}

// Same as above, but in the reused case.
TEST_F(URLRequestTest, InterceptLoadTimingEarlyProxyResolutionReused) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing = NormalLoadTimingInfoReused(now, true);
  job_load_timing.proxy_resolve_start = now - base::TimeDelta::FromDays(4);
  job_load_timing.proxy_resolve_end = now - base::TimeDelta::FromDays(3);

  LoadTimingInfo load_timing_result =
      RunLoadTimingTest(job_load_timing, &default_context_);

  // Proxy times and connect times should all be replaced with request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_end);

  // Other times should have been left null.
  TestLoadTimingReusedWithProxy(load_timing_result);
}

// Make sure that URLRequest correctly adjusts connect times when they're before
// |request_start|, due to reusing a connected socket.  The connected socket is
// not considered reused in this test (May be a preconnect).
//
// To mix things up, the request has SSL times, but no DNS times.
TEST_F(URLRequestTest, InterceptLoadTimingEarlyConnect) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_SSL_TIMES, false);
  job_load_timing.connect_timing.connect_start =
      now - base::TimeDelta::FromDays(1);
  job_load_timing.connect_timing.ssl_start = now - base::TimeDelta::FromDays(2);
  job_load_timing.connect_timing.ssl_end = now - base::TimeDelta::FromDays(3);
  job_load_timing.connect_timing.connect_end =
      now - base::TimeDelta::FromDays(4);

  LoadTimingInfo load_timing_result =
      RunLoadTimingTest(job_load_timing, &default_context_);

  // Connect times, and SSL times should be replaced with request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.ssl_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.ssl_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReused(load_timing_result, CONNECT_TIMING_HAS_SSL_TIMES);
}

// Make sure that URLRequest correctly adjusts connect times when they're before
// |request_start|, due to reusing a connected socket in the case that there
// are also proxy times.  The connected socket is not considered reused in this
// test (May be a preconnect).
//
// In this test, there are no SSL or DNS times.
TEST_F(URLRequestTest, InterceptLoadTimingEarlyConnectWithProxy) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY, true);
  job_load_timing.connect_timing.connect_start =
      now - base::TimeDelta::FromDays(1);
  job_load_timing.connect_timing.connect_end =
      now - base::TimeDelta::FromDays(2);

  LoadTimingInfo load_timing_result =
      RunLoadTimingTest(job_load_timing, &default_context_);

  // Connect times should be replaced with proxy_resolve_end.
  EXPECT_EQ(load_timing_result.proxy_resolve_end,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.proxy_resolve_end,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);
}

// Check that two different URL requests have different identifiers.
TEST_F(URLRequestTest, Identifiers) {
  TestDelegate d;
  TestURLRequestContext context;
  TestURLRequest req(
      GURL("http://example.com"), DEFAULT_PRIORITY, &d, &context);
  TestURLRequest other_req(
      GURL("http://example.com"), DEFAULT_PRIORITY, &d, &context);

  ASSERT_NE(req.identifier(), other_req.identifier());
}

// Check that a failure to connect to the proxy is reported to the network
// delegate.
TEST_F(URLRequestTest, NetworkDelegateProxyError) {
  MockHostResolver host_resolver;
  host_resolver.rules()->AddSimulatedFailure("*");

  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContextWithProxy context("myproxy:70", &network_delegate);

  TestDelegate d;
  URLRequest req(GURL("http://example.com"), DEFAULT_PRIORITY, &d, &context);
  req.set_method("GET");

  req.Start();
  base::RunLoop().Run();

  // Check we see a failed request.
  EXPECT_FALSE(req.status().is_success());
  // The proxy server is not set before failure.
  EXPECT_TRUE(req.proxy_server().IsEmpty());
  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, req.status().error());

  EXPECT_EQ(1, network_delegate.error_count());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, network_delegate.last_error());
  EXPECT_EQ(1, network_delegate.completed_requests());
}

// Make sure that net::NetworkDelegate::NotifyCompleted is called if
// content is empty.
TEST_F(URLRequestTest, RequestCompletionForEmptyResponse) {
  TestDelegate d;
  URLRequest req(GURL("data:,"), DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ("", d.data_received());
  EXPECT_EQ(1, default_network_delegate_.completed_requests());
}

// Make sure that SetPriority actually sets the URLRequest's priority
// correctly, both before and after start.
TEST_F(URLRequestTest, SetPriorityBasic) {
  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  EXPECT_EQ(DEFAULT_PRIORITY, req.priority());

  req.SetPriority(LOW);
  EXPECT_EQ(LOW, req.priority());

  req.Start();
  EXPECT_EQ(LOW, req.priority());

  req.SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, req.priority());
}

// Make sure that URLRequest calls SetPriority on a job before calling
// Start on it.
TEST_F(URLRequestTest, SetJobPriorityBeforeJobStart) {
  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  EXPECT_EQ(DEFAULT_PRIORITY, req.priority());

  scoped_refptr<URLRequestTestJob> job =
      new URLRequestTestJob(&req, &default_network_delegate_);
  AddTestInterceptor()->set_main_intercept_job(job.get());
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  req.SetPriority(LOW);

  req.Start();
  EXPECT_EQ(LOW, job->priority());
}

// Make sure that URLRequest passes on its priority updates to its
// job.
TEST_F(URLRequestTest, SetJobPriority) {
  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

  scoped_refptr<URLRequestTestJob> job =
      new URLRequestTestJob(&req, &default_network_delegate_);
  AddTestInterceptor()->set_main_intercept_job(job.get());

  req.SetPriority(LOW);
  req.Start();
  EXPECT_EQ(LOW, job->priority());

  req.SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, req.priority());
  EXPECT_EQ(MEDIUM, job->priority());
}

// Setting the IGNORE_LIMITS load flag should be okay if the priority
// is MAXIMUM_PRIORITY.
TEST_F(URLRequestTest, PriorityIgnoreLimits) {
  TestDelegate d;
  URLRequest req(GURL("http://test_intercept/foo"),
                 MAXIMUM_PRIORITY,
                 &d,
                 &default_context_);
  EXPECT_EQ(MAXIMUM_PRIORITY, req.priority());

  scoped_refptr<URLRequestTestJob> job =
      new URLRequestTestJob(&req, &default_network_delegate_);
  AddTestInterceptor()->set_main_intercept_job(job.get());

  req.SetLoadFlags(LOAD_IGNORE_LIMITS);
  EXPECT_EQ(MAXIMUM_PRIORITY, req.priority());

  req.SetPriority(MAXIMUM_PRIORITY);
  EXPECT_EQ(MAXIMUM_PRIORITY, req.priority());

  req.Start();
  EXPECT_EQ(MAXIMUM_PRIORITY, req.priority());
  EXPECT_EQ(MAXIMUM_PRIORITY, job->priority());
}

// TODO(droger): Support SpawnedTestServer on iOS (see http://crbug.com/148666).
#if !defined(OS_IOS)
// A subclass of SpawnedTestServer that uses a statically-configured hostname.
// This is to work around mysterious failures in chrome_frame_net_tests. See:
// http://crbug.com/114369
// TODO(erikwright): remove or update as needed; see http://crbug.com/334634.
class LocalHttpTestServer : public SpawnedTestServer {
 public:
  explicit LocalHttpTestServer(const base::FilePath& document_root)
      : SpawnedTestServer(SpawnedTestServer::TYPE_HTTP,
                          ScopedCustomUrlRequestTestHttpHost::value(),
                          document_root) {}
  LocalHttpTestServer()
      : SpawnedTestServer(SpawnedTestServer::TYPE_HTTP,
                          ScopedCustomUrlRequestTestHttpHost::value(),
                          base::FilePath()) {}
};

TEST_F(URLRequestTest, DelayedCookieCallback) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestURLRequestContext context;
  scoped_refptr<DelayedCookieMonster> delayed_cm =
      new DelayedCookieMonster();
  scoped_refptr<CookieStore> cookie_store = delayed_cm;
  context.set_cookie_store(delayed_cm.get());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    context.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"),
                   DEFAULT_PRIORITY,
                   &d,
                   &context);
    req.Start();
    base::RunLoop().Run();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(1, network_delegate.set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    context.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &context);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent when LOAD_DO_NOT_SEND_COOKIES is set.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.SetLoadFlags(LOAD_DO_NOT_SEND_COOKIES);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    // LOAD_DO_NOT_SEND_COOKIES does not trigger OnGetCookies.
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotUpdate=2"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(1, network_delegate.set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(
        test_server.GetURL("set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);
    req.SetLoadFlags(LOAD_DO_NOT_SAVE_COOKIES);
    req.Start();

    base::RunLoop().Run();

    // LOAD_DO_NOT_SAVE_COOKIES does not trigger OnSetCookie.
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(0, network_delegate.set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(0, network_delegate.set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_GET_COOKIES);
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    EXPECT_EQ(1, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotUpdate=2"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);
    URLRequest req(
        test_server.GetURL("set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);
    req.Start();

    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(2, network_delegate.blocked_set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveEmptyCookies) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up an empty cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(0, network_delegate.set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy_Async) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_GET_COOKIES);
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    EXPECT_EQ(1, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy_Async) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotUpdate=2"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);
    URLRequest req(
        test_server.GetURL("set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);
    req.Start();

    base::RunLoop().Run();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(2, network_delegate.blocked_set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

// FixedDateNetworkDelegate swaps out the server's HTTP Date response header
// value for the |fixed_date| argument given to the constructor.
class FixedDateNetworkDelegate : public TestNetworkDelegate {
 public:
  explicit FixedDateNetworkDelegate(const std::string& fixed_date)
      : fixed_date_(fixed_date) {}
  virtual ~FixedDateNetworkDelegate() {}

  // net::NetworkDelegate implementation
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE;

 private:
  std::string fixed_date_;

  DISALLOW_COPY_AND_ASSIGN(FixedDateNetworkDelegate);
};

int FixedDateNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  net::HttpResponseHeaders* new_response_headers =
      new net::HttpResponseHeaders(original_response_headers->raw_headers());

  new_response_headers->RemoveHeader("Date");
  new_response_headers->AddHeader("Date: " + fixed_date_);

  *override_response_headers = new_response_headers;
  return TestNetworkDelegate::OnHeadersReceived(request,
                                                callback,
                                                original_response_headers,
                                                override_response_headers,
                                                allowed_unsafe_redirect_url);
}

// Test that cookie expiration times are adjusted for server/client clock
// skew and that we handle incorrect timezone specifier "UTC" in HTTP Date
// headers by defaulting to GMT. (crbug.com/135131)
TEST_F(URLRequestTest, AcceptClockSkewCookieWithWrongDateTimezone) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up an expired cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(
        test_server.GetURL(
            "set-cookie?StillGood=1;expires=Mon,18-Apr-1977,22:50:13,GMT"),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);
    req.Start();
    base::RunLoop().Run();
  }
  // Verify that the cookie is not set.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("StillGood=1") == std::string::npos);
  }
  // Set up a cookie with clock skew and "UTC" HTTP Date timezone specifier.
  {
    FixedDateNetworkDelegate network_delegate("18-Apr-1977 22:49:13 UTC");
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(
        test_server.GetURL(
            "set-cookie?StillGood=1;expires=Mon,18-Apr-1977,22:50:13,GMT"),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);
    req.Start();
    base::RunLoop().Run();
  }
  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context_.set_network_delegate(&network_delegate);
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Cookie"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("StillGood=1") != std::string::npos);
  }
}


// Check that it is impossible to change the referrer in the extra headers of
// an URLRequest.
TEST_F(URLRequestTest, DoNotOverrideReferrer) {
  LocalHttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // If extra headers contain referer and the request contains a referer,
  // only the latter shall be respected.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Referer"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.SetReferrer("http://foo.com/");

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kReferer, "http://bar.com/");
    req.SetExtraRequestHeaders(headers);

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ("http://foo.com/", d.data_received());
  }

  // If extra headers contain a referer but the request does not, no referer
  // shall be sent in the header.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("echoheader?Referer"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kReferer, "http://bar.com/");
    req.SetExtraRequestHeaders(headers);
    req.SetLoadFlags(LOAD_VALIDATE_CACHE);

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ("None", d.data_received());
  }
}

class URLRequestTestHTTP : public URLRequestTest {
 public:
  URLRequestTestHTTP()
      : test_server_(base::FilePath(FILE_PATH_LITERAL(
                                  "net/data/url_request_unittest"))) {
  }

 protected:
  // Requests |redirect_url|, which must return a HTTP 3xx redirect.
  // |request_method| is the method to use for the initial request.
  // |redirect_method| is the method that is expected to be used for the second
  // request, after redirection.
  // If |include_data| is true, data is uploaded with the request.  The
  // response body is expected to match it exactly, if and only if
  // |request_method| == |redirect_method|.
  void HTTPRedirectMethodTest(const GURL& redirect_url,
                              const std::string& request_method,
                              const std::string& redirect_method,
                              bool include_data) {
    static const char kData[] = "hello world";
    TestDelegate d;
    URLRequest req(redirect_url, DEFAULT_PRIORITY, &d, &default_context_);
    req.set_method(request_method);
    if (include_data) {
      req.set_upload(make_scoped_ptr(CreateSimpleUploadData(kData)));
      HttpRequestHeaders headers;
      headers.SetHeader(HttpRequestHeaders::kContentLength,
                        base::UintToString(arraysize(kData) - 1));
      req.SetExtraRequestHeaders(headers);
    }
    req.Start();
    base::RunLoop().Run();
    EXPECT_EQ(redirect_method, req.method());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
    EXPECT_EQ(OK, req.status().error());
    if (include_data) {
      if (request_method == redirect_method) {
        EXPECT_EQ(kData, d.data_received());
      } else {
        EXPECT_NE(kData, d.data_received());
      }
    }
    if (HasFailure())
      LOG(WARNING) << "Request method was: " << request_method;
  }

  void HTTPUploadDataOperationTest(const std::string& method) {
    const int kMsgSize = 20000;  // multiple of 10
    const int kIterations = 50;
    char* uploadBytes = new char[kMsgSize+1];
    char* ptr = uploadBytes;
    char marker = 'a';
    for (int idx = 0; idx < kMsgSize/10; idx++) {
      memcpy(ptr, "----------", 10);
      ptr += 10;
      if (idx % 100 == 0) {
        ptr--;
        *ptr++ = marker;
        if (++marker > 'z')
          marker = 'a';
      }
    }
    uploadBytes[kMsgSize] = '\0';

    for (int i = 0; i < kIterations; ++i) {
      TestDelegate d;
      URLRequest r(
          test_server_.GetURL("echo"), DEFAULT_PRIORITY, &d, &default_context_);
      r.set_method(method.c_str());

      r.set_upload(make_scoped_ptr(CreateSimpleUploadData(uploadBytes)));

      r.Start();
      EXPECT_TRUE(r.is_pending());

      base::RunLoop().Run();

      ASSERT_EQ(1, d.response_started_count())
          << "request failed: " << r.status().status()
          << ", os error: " << r.status().error();

      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_EQ(uploadBytes, d.data_received());
    }
    delete[] uploadBytes;
  }

  void AddChunksToUpload(URLRequest* r) {
    r->AppendChunkToUpload("a", 1, false);
    r->AppendChunkToUpload("bcd", 3, false);
    r->AppendChunkToUpload("this is a longer chunk than before.", 35, false);
    r->AppendChunkToUpload("\r\n\r\n", 4, false);
    r->AppendChunkToUpload("0", 1, false);
    r->AppendChunkToUpload("2323", 4, true);
  }

  void VerifyReceivedDataMatchesChunks(URLRequest* r, TestDelegate* d) {
    // This should match the chunks sent by AddChunksToUpload().
    const std::string expected_data =
        "abcdthis is a longer chunk than before.\r\n\r\n02323";

    ASSERT_EQ(1, d->response_started_count())
        << "request failed: " << r->status().status()
        << ", os error: " << r->status().error();

    EXPECT_FALSE(d->received_data_before_response());

    EXPECT_EQ(expected_data.size(), static_cast<size_t>(d->bytes_received()));
    EXPECT_EQ(expected_data, d->data_received());
  }

  bool DoManyCookiesRequest(int num_cookies) {
    TestDelegate d;
    URLRequest r(test_server_.GetURL("set-many-cookies?" +
                                     base::IntToString(num_cookies)),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    bool is_success = r.status().is_success();

    if (!is_success) {
      EXPECT_TRUE(r.status().error() == ERR_RESPONSE_HEADERS_TOO_BIG);
      // The test server appears to be unable to handle subsequent requests
      // after this error is triggered. Force it to restart.
      EXPECT_TRUE(test_server_.Stop());
      EXPECT_TRUE(test_server_.Start());
    }

    return is_success;
  }

  LocalHttpTestServer test_server_;
};

// In this unit test, we're using the HTTPTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.redirect.com".
// The HTTPTestServer will return a 302 response, which we should not
// follow.
TEST_F(URLRequestTestHTTP, ProxyTunnelRedirectTest) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  TestDelegate d;
  {
    URLRequest r(
        GURL("https://www.redirect.com/"), DEFAULT_PRIORITY, &d, &context);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    // The proxy server is not set before failure.
    EXPECT_TRUE(r.proxy_server().IsEmpty());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, r.status().error());
    EXPECT_EQ(1, d.response_started_count());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());
  }
}

// This is the same as the previous test, but checks that the network delegate
// registers the error.
TEST_F(URLRequestTestHTTP, NetworkDelegateTunnelConnectionFailed) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  TestDelegate d;
  {
    URLRequest r(
        GURL("https://www.redirect.com/"), DEFAULT_PRIORITY, &d, &context);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    // The proxy server is not set before failure.
    EXPECT_TRUE(r.proxy_server().IsEmpty());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, r.status().error());
    EXPECT_EQ(1, d.response_started_count());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());

    EXPECT_EQ(1, network_delegate.error_count());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, network_delegate.last_error());
  }
}

// Tests that we can block and asynchronously return OK in various stages.
TEST_F(URLRequestTestHTTP, NetworkDelegateBlockAsynchronously) {
  static const BlockingNetworkDelegate::Stage blocking_stages[] = {
    BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
    BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
    BlockingNetworkDelegate::ON_HEADERS_RECEIVED
  };
  static const size_t blocking_stages_length = arraysize(blocking_stages);

  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(
      BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST |
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS |
      BlockingNetworkDelegate::ON_HEADERS_RECEIVED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    URLRequest r(
        test_server_.GetURL("empty.html"), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    for (size_t i = 0; i < blocking_stages_length; ++i) {
      base::RunLoop().Run();
      EXPECT_EQ(blocking_stages[i],
                network_delegate.stage_blocked_for_callback());
      network_delegate.DoCallback(OK);
    }
    base::RunLoop().Run();
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and cancel a request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  network_delegate.set_retval(ERR_EMPTY_RESPONSE);

  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  {
    URLRequest r(
        test_server_.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    // The proxy server is not set before cancellation.
    EXPECT_TRUE(r.proxy_server().IsEmpty());
    EXPECT_EQ(ERR_EMPTY_RESPONSE, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Helper function for NetworkDelegateCancelRequestAsynchronously and
// NetworkDelegateCancelRequestSynchronously. Sets up a blocking network
// delegate operating in |block_mode| and a request for |url|. It blocks the
// request in |stage| and cancels it with ERR_BLOCKED_BY_CLIENT.
void NetworkDelegateCancelRequest(BlockingNetworkDelegate::BlockMode block_mode,
                                  BlockingNetworkDelegate::Stage stage,
                                  const GURL& url) {
  TestDelegate d;
  BlockingNetworkDelegate network_delegate(block_mode);
  network_delegate.set_retval(ERR_BLOCKED_BY_CLIENT);
  network_delegate.set_block_on(stage);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    // The proxy server is not set before cancellation.
    EXPECT_TRUE(r.proxy_server().IsEmpty());
    EXPECT_EQ(ERR_BLOCKED_BY_CLIENT, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// The following 3 tests check that the network delegate can cancel a request
// synchronously in various stages of the request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously1) {
  ASSERT_TRUE(test_server_.Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::SYNCHRONOUS,
                               BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
                               test_server_.GetURL(std::string()));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously2) {
  ASSERT_TRUE(test_server_.Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::SYNCHRONOUS,
                               BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
                               test_server_.GetURL(std::string()));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously3) {
  ASSERT_TRUE(test_server_.Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::SYNCHRONOUS,
                               BlockingNetworkDelegate::ON_HEADERS_RECEIVED,
                               test_server_.GetURL(std::string()));
}

// The following 3 tests check that the network delegate can cancel a request
// asynchronously in various stages of the request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestAsynchronously1) {
  ASSERT_TRUE(test_server_.Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::AUTO_CALLBACK,
                               BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
                               test_server_.GetURL(std::string()));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestAsynchronously2) {
  ASSERT_TRUE(test_server_.Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::AUTO_CALLBACK,
                               BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
                               test_server_.GetURL(std::string()));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestAsynchronously3) {
  ASSERT_TRUE(test_server_.Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::AUTO_CALLBACK,
                               BlockingNetworkDelegate::ON_HEADERS_RECEIVED,
                               test_server_.GetURL(std::string()));
}

// Tests that the network delegate can block and redirect a request to a new
// URL.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  GURL redirect_url(test_server_.GetURL("simple.html"));
  network_delegate.set_redirect_url(redirect_url);

  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  {
    GURL original_url(test_server_.GetURL("empty.html"));
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_TRUE(r.proxy_server().Equals(test_server_.host_port_pair()));
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(redirect_url, r.url());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and redirect a request to a new
// URL by setting a redirect_url and returning in OnBeforeURLRequest directly.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestSynchronously) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  GURL redirect_url(test_server_.GetURL("simple.html"));
  network_delegate.set_redirect_url(redirect_url);

  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  {
    GURL original_url(test_server_.GetURL("empty.html"));
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_TRUE(r.proxy_server().Equals(test_server_.host_port_pair()));
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(redirect_url, r.url());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that redirects caused by the network delegate preserve POST data.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestPost) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  GURL redirect_url(test_server_.GetURL("echo"));
  network_delegate.set_redirect_url(redirect_url);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL original_url(test_server_.GetURL("empty.html"));
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &context);
    r.set_method("POST");
    r.set_upload(make_scoped_ptr(CreateSimpleUploadData(kData)));
    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kContentLength,
                      base::UintToString(arraysize(kData) - 1));
    r.SetExtraRequestHeaders(headers);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(redirect_url, r.url());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
    EXPECT_EQ("POST", r.method());
    EXPECT_EQ(kData, d.data_received());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and redirect a request to a new
// URL during OnHeadersReceived.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestOnHeadersReceived) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_HEADERS_RECEIVED);
  GURL redirect_url(test_server_.GetURL("simple.html"));
  network_delegate.set_redirect_on_headers_received_url(redirect_url);

  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  {
    GURL original_url(test_server_.GetURL("empty.html"));
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_TRUE(r.proxy_server().Equals(test_server_.host_port_pair()));
    EXPECT_EQ(net::OK, r.status().error());
    EXPECT_EQ(redirect_url, r.url());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(2, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by taking no action. This indicates that the NetworkDelegate does not want to
// handle the challenge, and is passing the buck along to the
// URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncNoAction) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();

    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

TEST_F(URLRequestTestHTTP,
    NetworkDelegateOnAuthRequiredSyncNoAction_GetFullRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();

    {
      HttpRequestHeaders headers;
      EXPECT_TRUE(r.GetFullRequestHeaders(&headers));
      EXPECT_FALSE(headers.HasHeader("Authorization"));
    }

    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by setting credentials.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncSetAuth) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  network_delegate.set_auth_credentials(AuthCredentials(kUser, kSecret));

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Same as above, but also tests that GetFullRequestHeaders returns the proper
// headers (for the first or second request) when called at the proper times.
TEST_F(URLRequestTestHTTP,
    NetworkDelegateOnAuthRequiredSyncSetAuth_GetFullRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  network_delegate.set_auth_credentials(AuthCredentials(kUser, kSecret));

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());

    {
      HttpRequestHeaders headers;
      EXPECT_TRUE(r.GetFullRequestHeaders(&headers));
      EXPECT_TRUE(headers.HasHeader("Authorization"));
    }
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by cancelling authentication.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncCancel) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(OK, r.status().error());
    EXPECT_EQ(401, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by taking no action. This indicates that the NetworkDelegate does not want
// to handle the challenge, and is passing the buck along to the
// URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncNoAction) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by setting credentials.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncSetAuth) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  AuthCredentials auth_credentials(kUser, kSecret);
  network_delegate.set_auth_credentials(auth_credentials);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by cancelling authentication.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncCancel) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    URLRequest r(url, DEFAULT_PRIORITY, &d, &context);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(OK, r.status().error());
    EXPECT_EQ(401, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 1: Request is cancelled while waiting for OnBeforeURLRequest callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting1) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    URLRequest r(
        test_server_.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();
    EXPECT_EQ(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r.Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(ERR_ABORTED, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 2: Request is cancelled while waiting for OnBeforeSendHeaders callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting2) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    URLRequest r(
        test_server_.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();
    EXPECT_EQ(BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r.Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(ERR_ABORTED, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 3: Request is cancelled while waiting for OnHeadersReceived callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting3) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_HEADERS_RECEIVED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    URLRequest r(
        test_server_.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();
    EXPECT_EQ(BlockingNetworkDelegate::ON_HEADERS_RECEIVED,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r.Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(ERR_ABORTED, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 4: Request is cancelled while waiting for OnAuthRequired callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting4) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    URLRequest r(
        test_server_.GetURL("auth-basic"), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    base::RunLoop().Run();
    EXPECT_EQ(BlockingNetworkDelegate::ON_AUTH_REQUIRED,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r.Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(ERR_ABORTED, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// In this unit test, we're using the HTTPTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.server-auth.com".
// The HTTPTestServer will return a 401 response, which we should balk at.
TEST_F(URLRequestTestHTTP, UnexpectedServerAuthTest) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContextWithProxy context(
      test_server_.host_port_pair().ToString(), &network_delegate);

  TestDelegate d;
  {
    URLRequest r(
        GURL("https://www.server-auth.com/"), DEFAULT_PRIORITY, &d, &context);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    // The proxy server is not set before failure.
    EXPECT_TRUE(r.proxy_server().IsEmpty());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, r.status().error());
  }
}

TEST_F(URLRequestTestHTTP, GetTest_NoCache) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());

    // TODO(eroman): Add back the NetLog tests...
  }
}

// This test has the server send a large number of cookies to the client.
// To ensure that no number of cookies causes a crash, a galloping binary
// search is used to estimate that maximum number of cookies that are accepted
// by the browser. Beyond the maximum number, the request will fail with
// ERR_RESPONSE_HEADERS_TOO_BIG.
#if defined(OS_WIN)
// http://crbug.com/177916
#define MAYBE_GetTest_ManyCookies DISABLED_GetTest_ManyCookies
#else
#define MAYBE_GetTest_ManyCookies GetTest_ManyCookies
#endif  // defined(OS_WIN)
TEST_F(URLRequestTestHTTP, MAYBE_GetTest_ManyCookies) {
  ASSERT_TRUE(test_server_.Start());

  int lower_bound = 0;
  int upper_bound = 1;

  // Double the number of cookies until the response header limits are
  // exceeded.
  while (DoManyCookiesRequest(upper_bound)) {
    lower_bound = upper_bound;
    upper_bound *= 2;
    ASSERT_LT(upper_bound, 1000000);
  }

  int tolerance = upper_bound * 0.005;
  if (tolerance < 2)
    tolerance = 2;

  // Perform a binary search to find the highest possible number of cookies,
  // within the desired tolerance.
  while (upper_bound - lower_bound >= tolerance) {
    int num_cookies = (lower_bound + upper_bound) / 2;

    if (DoManyCookiesRequest(num_cookies))
      lower_bound = num_cookies;
    else
      upper_bound = num_cookies;
  }
  // Success: the test did not crash.
}

TEST_F(URLRequestTestHTTP, GetTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

TEST_F(URLRequestTestHTTP, GetTest_GetFullRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    GURL test_url(test_server_.GetURL(std::string()));
    URLRequest r(test_url, DEFAULT_PRIORITY, &d, &default_context_);

    HttpRequestHeaders headers;
    EXPECT_FALSE(r.GetFullRequestHeaders(&headers));

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());

    EXPECT_TRUE(d.have_full_request_headers());
    CheckFullRequestHeaders(d.full_request_headers(), test_url);
  }
}

TEST_F(URLRequestTestHTTP, GetTestLoadTiming) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    LoadTimingInfo load_timing_info;
    r.GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

TEST_F(URLRequestTestHTTP, GetZippedTest) {
  ASSERT_TRUE(test_server_.Start());

  // Parameter that specifies the Content-Length field in the response:
  // C - Compressed length.
  // U - Uncompressed length.
  // L - Large length (larger than both C & U).
  // M - Medium length (between C & U).
  // S - Small length (smaller than both C & U).
  const char test_parameters[] = "CULMS";
  const int num_tests = arraysize(test_parameters)- 1;  // Skip NULL.
  // C & U should be OK.
  // L & M are larger than the data sent, and show an error.
  // S has too little data, but we seem to accept it.
  const bool test_expect_success[num_tests] =
      { true, true, false, false, true };

  for (int i = 0; i < num_tests ; i++) {
    TestDelegate d;
    {
      std::string test_file =
          base::StringPrintf("compressedfiles/BullRunSpeech.txt?%c",
                             test_parameters[i]);

      TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
      TestURLRequestContext context(true);
      context.set_network_delegate(&network_delegate);
      context.Init();

      URLRequest r(
          test_server_.GetURL(test_file), DEFAULT_PRIORITY, &d, &context);
      r.Start();
      EXPECT_TRUE(r.is_pending());

      base::RunLoop().Run();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      VLOG(1) << " Received " << d.bytes_received() << " bytes"
              << " status = " << r.status().status()
              << " error = " << r.status().error();
      if (test_expect_success[i]) {
        EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status())
            << " Parameter = \"" << test_file << "\"";
      } else {
        EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
        EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, r.status().error())
            << " Parameter = \"" << test_file << "\"";
      }
    }
  }
}

TEST_F(URLRequestTestHTTP, HTTPSToHTTPRedirectNoRefererTest) {
  ASSERT_TRUE(test_server_.Start());

  SpawnedTestServer https_test_server(
      SpawnedTestServer::TYPE_HTTPS, SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(https_test_server.Start());

  // An https server is sent a request with an https referer,
  // and responds with a redirect to an http url. The http
  // server should not be sent the referer.
  GURL http_destination = test_server_.GetURL(std::string());
  TestDelegate d;
  URLRequest req(
      https_test_server.GetURL("server-redirect?" + http_destination.spec()),
      DEFAULT_PRIORITY,
      &d,
      &default_context_);
  req.SetReferrer("https://www.referrer.com/");
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(1, d.received_redirect_count());
  EXPECT_EQ(http_destination, req.url());
  EXPECT_EQ(std::string(), req.referrer());
}

TEST_F(URLRequestTestHTTP, RedirectLoadTiming) {
  ASSERT_TRUE(test_server_.Start());

  GURL destination_url = test_server_.GetURL(std::string());
  GURL original_url =
      test_server_.GetURL("server-redirect?" + destination_url.spec());
  TestDelegate d;
  URLRequest req(original_url, DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(1, d.received_redirect_count());
  EXPECT_EQ(destination_url, req.url());
  EXPECT_EQ(original_url, req.original_url());
  ASSERT_EQ(2U, req.url_chain().size());
  EXPECT_EQ(original_url, req.url_chain()[0]);
  EXPECT_EQ(destination_url, req.url_chain()[1]);

  LoadTimingInfo load_timing_info_before_redirect;
  EXPECT_TRUE(default_network_delegate_.GetLoadTimingInfoBeforeRedirect(
      &load_timing_info_before_redirect));
  TestLoadTimingNotReused(load_timing_info_before_redirect,
                          CONNECT_TIMING_HAS_DNS_TIMES);

  LoadTimingInfo load_timing_info;
  req.GetLoadTimingInfo(&load_timing_info);
  TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);

  // Check that a new socket was used on redirect, since the server does not
  // supposed keep-alive sockets, and that the times before the redirect are
  // before the ones recorded for the second request.
  EXPECT_NE(load_timing_info_before_redirect.socket_log_id,
            load_timing_info.socket_log_id);
  EXPECT_LE(load_timing_info_before_redirect.receive_headers_end,
            load_timing_info.connect_timing.connect_start);
}

TEST_F(URLRequestTestHTTP, MultipleRedirectTest) {
  ASSERT_TRUE(test_server_.Start());

  GURL destination_url = test_server_.GetURL(std::string());
  GURL middle_redirect_url =
      test_server_.GetURL("server-redirect?" + destination_url.spec());
  GURL original_url = test_server_.GetURL(
      "server-redirect?" + middle_redirect_url.spec());
  TestDelegate d;
  URLRequest req(original_url, DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(2, d.received_redirect_count());
  EXPECT_EQ(destination_url, req.url());
  EXPECT_EQ(original_url, req.original_url());
  ASSERT_EQ(3U, req.url_chain().size());
  EXPECT_EQ(original_url, req.url_chain()[0]);
  EXPECT_EQ(middle_redirect_url, req.url_chain()[1]);
  EXPECT_EQ(destination_url, req.url_chain()[2]);
}

// First and second pieces of information logged by delegates to URLRequests.
const char kFirstDelegateInfo[] = "Wonderful delegate";
const char kSecondDelegateInfo[] = "Exciting delegate";

// Logs delegate information to a URLRequest.  The first string is logged
// synchronously on Start(), using DELEGATE_INFO_DEBUG_ONLY.  The second is
// logged asynchronously, using DELEGATE_INFO_DISPLAY_TO_USER.  Then
// another asynchronous call is used to clear the delegate information
// before calling a callback.  The object then deletes itself.
class AsyncDelegateLogger : public base::RefCounted<AsyncDelegateLogger> {
 public:
  typedef base::Callback<void()> Callback;

  // Each time delegate information is added to the URLRequest, the resulting
  // load state is checked.  The expected load state after each request is
  // passed in as an argument.
  static void Run(URLRequest* url_request,
                  LoadState expected_first_load_state,
                  LoadState expected_second_load_state,
                  LoadState expected_third_load_state,
                  const Callback& callback) {
    AsyncDelegateLogger* logger = new AsyncDelegateLogger(
        url_request,
        expected_first_load_state,
        expected_second_load_state,
        expected_third_load_state,
        callback);
    logger->Start();
  }

  // Checks that the log entries, starting with log_position, contain the
  // DELEGATE_INFO NetLog events that an AsyncDelegateLogger should have
  // recorded.  Returns the index of entry after the expected number of
  // events this logged, or entries.size() if there aren't enough entries.
  static size_t CheckDelegateInfo(
      const CapturingNetLog::CapturedEntryList& entries, size_t log_position) {
    // There should be 4 DELEGATE_INFO events: Two begins and two ends.
    if (log_position + 3 >= entries.size()) {
      ADD_FAILURE() << "Not enough log entries";
      return entries.size();
    }
    std::string delegate_info;
    EXPECT_EQ(NetLog::TYPE_DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_BEGIN, entries[log_position].phase);
    EXPECT_TRUE(entries[log_position].GetStringValue("delegate_info",
                                                     &delegate_info));
    EXPECT_EQ(kFirstDelegateInfo, delegate_info);

    ++log_position;
    EXPECT_EQ(NetLog::TYPE_DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);

    ++log_position;
    EXPECT_EQ(NetLog::TYPE_DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_BEGIN, entries[log_position].phase);
    EXPECT_TRUE(entries[log_position].GetStringValue("delegate_info",
                                                     &delegate_info));
    EXPECT_EQ(kSecondDelegateInfo, delegate_info);

    ++log_position;
    EXPECT_EQ(NetLog::TYPE_DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);

    return log_position + 1;
  }

  // Find delegate request begin and end messages for OnBeforeNetworkStart.
  // Returns the position of the end message.
  static size_t ExpectBeforeNetworkEvents(
      const CapturingNetLog::CapturedEntryList& entries,
      size_t log_position) {
    log_position =
        ExpectLogContainsSomewhereAfter(entries,
                                        log_position,
                                        NetLog::TYPE_URL_REQUEST_DELEGATE,
                                        NetLog::PHASE_BEGIN);
    EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE,
              entries[log_position + 1].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position + 1].phase);
    return log_position + 1;
  }

 private:
  friend class base::RefCounted<AsyncDelegateLogger>;

  AsyncDelegateLogger(URLRequest* url_request,
                      LoadState expected_first_load_state,
                      LoadState expected_second_load_state,
                      LoadState expected_third_load_state,
                      const Callback& callback)
      : url_request_(url_request),
        expected_first_load_state_(expected_first_load_state),
        expected_second_load_state_(expected_second_load_state),
        expected_third_load_state_(expected_third_load_state),
        callback_(callback) {
  }

  ~AsyncDelegateLogger() {}

  void Start() {
    url_request_->LogBlockedBy(kFirstDelegateInfo);
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_first_load_state_, load_state.state);
    EXPECT_NE(ASCIIToUTF16(kFirstDelegateInfo), load_state.param);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AsyncDelegateLogger::LogSecondDelegate, this));
  }

  void LogSecondDelegate() {
    url_request_->LogAndReportBlockedBy(kSecondDelegateInfo);
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_second_load_state_, load_state.state);
    if (expected_second_load_state_ == LOAD_STATE_WAITING_FOR_DELEGATE) {
      EXPECT_EQ(ASCIIToUTF16(kSecondDelegateInfo), load_state.param);
    } else {
      EXPECT_NE(ASCIIToUTF16(kSecondDelegateInfo), load_state.param);
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AsyncDelegateLogger::LogComplete, this));
  }

  void LogComplete() {
    url_request_->LogUnblocked();
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_third_load_state_, load_state.state);
    if (expected_second_load_state_ == LOAD_STATE_WAITING_FOR_DELEGATE)
      EXPECT_EQ(base::string16(), load_state.param);
    callback_.Run();
  }

  URLRequest* url_request_;
  const int expected_first_load_state_;
  const int expected_second_load_state_;
  const int expected_third_load_state_;
  const Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDelegateLogger);
};

// NetworkDelegate that logs delegate information before a request is started,
// before headers are sent, when headers are read, and when auth information
// is requested.  Uses AsyncDelegateLogger.
class AsyncLoggingNetworkDelegate : public TestNetworkDelegate {
 public:
  AsyncLoggingNetworkDelegate() {}
  virtual ~AsyncLoggingNetworkDelegate() {}

  // NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 const CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE {
    TestNetworkDelegate::OnBeforeURLRequest(request, callback, new_url);
    return RunCallbackAsynchronously(request, callback);
  }

  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) OVERRIDE {
    TestNetworkDelegate::OnBeforeSendHeaders(request, callback, headers);
    return RunCallbackAsynchronously(request, callback);
  }

  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE {
    TestNetworkDelegate::OnHeadersReceived(request,
                                           callback,
                                           original_response_headers,
                                           override_response_headers,
                                           allowed_unsafe_redirect_url);
    return RunCallbackAsynchronously(request, callback);
  }

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) OVERRIDE {
    AsyncDelegateLogger::Run(
        request,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        base::Bind(&AsyncLoggingNetworkDelegate::SetAuthAndResume,
                   callback, credentials));
    return AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }

 private:
  static int RunCallbackAsynchronously(
      URLRequest* request,
      const CompletionCallback& callback) {
    AsyncDelegateLogger::Run(
        request,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        base::Bind(callback, OK));
    return ERR_IO_PENDING;
  }

  static void SetAuthAndResume(const AuthCallback& callback,
                               AuthCredentials* credentials) {
    *credentials = AuthCredentials(kUser, kSecret);
    callback.Run(NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);
  }

  DISALLOW_COPY_AND_ASSIGN(AsyncLoggingNetworkDelegate);
};

// URLRequest::Delegate that logs delegate information when the headers
// are received, when each read completes, and during redirects.  Uses
// AsyncDelegateLogger.  Can optionally cancel a request in any phase.
//
// Inherits from TestDelegate to reuse the TestDelegate code to handle
// advancing to the next step in most cases, as well as cancellation.
class AsyncLoggingUrlRequestDelegate : public TestDelegate {
 public:
  enum CancelStage {
    NO_CANCEL = 0,
    CANCEL_ON_RECEIVED_REDIRECT,
    CANCEL_ON_RESPONSE_STARTED,
    CANCEL_ON_READ_COMPLETED
  };

  explicit AsyncLoggingUrlRequestDelegate(CancelStage cancel_stage)
      : cancel_stage_(cancel_stage) {
    if (cancel_stage == CANCEL_ON_RECEIVED_REDIRECT)
      set_cancel_in_received_redirect(true);
    else if (cancel_stage == CANCEL_ON_RESPONSE_STARTED)
      set_cancel_in_response_started(true);
    else if (cancel_stage == CANCEL_ON_READ_COMPLETED)
      set_cancel_in_received_data(true);
  }
  virtual ~AsyncLoggingUrlRequestDelegate() {}

  // URLRequest::Delegate implementation:
  void virtual OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE {
    *defer_redirect = true;
    AsyncDelegateLogger::Run(
        request,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        base::Bind(
            &AsyncLoggingUrlRequestDelegate::OnReceivedRedirectLoggingComplete,
            base::Unretained(this), request, new_url));
  }

  virtual void OnResponseStarted(URLRequest* request) OVERRIDE {
    AsyncDelegateLogger::Run(
      request,
      LOAD_STATE_WAITING_FOR_DELEGATE,
      LOAD_STATE_WAITING_FOR_DELEGATE,
      LOAD_STATE_WAITING_FOR_DELEGATE,
      base::Bind(
          &AsyncLoggingUrlRequestDelegate::OnResponseStartedLoggingComplete,
          base::Unretained(this), request));
  }

  virtual void OnReadCompleted(URLRequest* request,
                               int bytes_read) OVERRIDE {
    AsyncDelegateLogger::Run(
        request,
        LOAD_STATE_IDLE,
        LOAD_STATE_IDLE,
        LOAD_STATE_IDLE,
        base::Bind(
            &AsyncLoggingUrlRequestDelegate::AfterReadCompletedLoggingComplete,
            base::Unretained(this), request, bytes_read));
  }

 private:
  void OnReceivedRedirectLoggingComplete(URLRequest* request,
                                         const GURL& new_url) {
    bool defer_redirect = false;
    TestDelegate::OnReceivedRedirect(request, new_url, &defer_redirect);
    // FollowDeferredRedirect should not be called after cancellation.
    if (cancel_stage_ == CANCEL_ON_RECEIVED_REDIRECT)
      return;
    if (!defer_redirect)
      request->FollowDeferredRedirect();
  }

  void OnResponseStartedLoggingComplete(URLRequest* request) {
    // The parent class continues the request.
    TestDelegate::OnResponseStarted(request);
  }

  void AfterReadCompletedLoggingComplete(URLRequest* request, int bytes_read) {
    // The parent class continues the request.
    TestDelegate::OnReadCompleted(request, bytes_read);
  }

  const CancelStage cancel_stage_;

  DISALLOW_COPY_AND_ASSIGN(AsyncLoggingUrlRequestDelegate);
};

// Tests handling of delegate info before a request starts.
TEST_F(URLRequestTestHTTP, DelegateInfoBeforeStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate request_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(NULL);
  context.set_net_log(&net_log_);
  context.Init();

  {
    URLRequest r(test_server_.GetURL("empty.html"),
                 DEFAULT_PRIORITY,
                 &request_delegate,
                 &context);
    LoadStateWithParam load_state = r.GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    AsyncDelegateLogger::Run(
        &r,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_IDLE,
        base::Bind(&URLRequest::Start, base::Unretained(&r)));

    base::RunLoop().Run();

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  }

  CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  size_t log_position = ExpectLogContainsSomewhereAfter(
      entries,
      0,
      NetLog::TYPE_DELEGATE_INFO,
      NetLog::PHASE_BEGIN);

  log_position = AsyncDelegateLogger::CheckDelegateInfo(entries, log_position);

  // Nothing else should add any delegate info to the request.
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
}

// Tests handling of delegate info from a network delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateInfo) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate request_delegate;
  AsyncLoggingNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_net_log(&net_log_);
  context.Init();

  {
    URLRequest r(test_server_.GetURL("simple.html"),
                 DEFAULT_PRIORITY,
                 &request_delegate,
                 &context);
    LoadStateWithParam load_state = r.GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  for (size_t i = 0; i < 3; ++i) {
    log_position = ExpectLogContainsSomewhereAfter(
        entries,
        log_position + 1,
        NetLog::TYPE_URL_REQUEST_DELEGATE,
        NetLog::PHASE_BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);

    if (i == 1) {
      log_position = AsyncDelegateLogger::ExpectBeforeNetworkEvents(
          entries, log_position + 1);
    }
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
}

// Tests handling of delegate info from a network delegate in the case of an
// HTTP redirect.
TEST_F(URLRequestTestHTTP, NetworkDelegateInfoRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate request_delegate;
  AsyncLoggingNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_net_log(&net_log_);
  context.Init();

  {
    URLRequest r(test_server_.GetURL("server-redirect?simple.html"),
                 DEFAULT_PRIORITY,
                 &request_delegate,
                 &context);
    LoadStateWithParam load_state = r.GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(2, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  // The NetworkDelegate logged information in OnBeforeURLRequest,
  // OnBeforeSendHeaders, and OnHeadersReceived.
  for (size_t i = 0; i < 3; ++i) {
    log_position = ExpectLogContainsSomewhereAfter(
        entries,
        log_position + 1,
        NetLog::TYPE_URL_REQUEST_DELEGATE,
        NetLog::PHASE_BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);

    if (i == 1) {
      log_position = AsyncDelegateLogger::ExpectBeforeNetworkEvents(
          entries, log_position + 1);
    }
  }

  // The URLRequest::Delegate then gets informed about the redirect.
  log_position = ExpectLogContainsSomewhereAfter(
      entries,
      log_position + 1,
      NetLog::TYPE_URL_REQUEST_DELEGATE,
      NetLog::PHASE_BEGIN);

  // The NetworkDelegate logged information in the same three events as before.
  for (size_t i = 0; i < 3; ++i) {
    log_position = ExpectLogContainsSomewhereAfter(
        entries,
        log_position + 1,
        NetLog::TYPE_URL_REQUEST_DELEGATE,
        NetLog::PHASE_BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
}

// Tests handling of delegate info from a network delegate in the case of HTTP
// AUTH.
TEST_F(URLRequestTestHTTP, NetworkDelegateInfoAuth) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate request_delegate;
  AsyncLoggingNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_net_log(&net_log_);
  context.Init();

  {
    URLRequest r(test_server_.GetURL("auth-basic"),
                 DEFAULT_PRIORITY,
                 &request_delegate,
                 &context);
    LoadStateWithParam load_state = r.GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  // The NetworkDelegate should have logged information in OnBeforeURLRequest,
  // OnBeforeSendHeaders, OnHeadersReceived, OnAuthRequired, and then again in
  // OnBeforeURLRequest and OnBeforeSendHeaders.
  for (size_t i = 0; i < 6; ++i) {
    log_position = ExpectLogContainsSomewhereAfter(
        entries,
        log_position + 1,
        NetLog::TYPE_URL_REQUEST_DELEGATE,
        NetLog::PHASE_BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);

    if (i == 1) {
      log_position = AsyncDelegateLogger::ExpectBeforeNetworkEvents(
          entries, log_position + 1);
    }
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
}

// Tests handling of delegate info from a URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, URLRequestDelegateInfo) {
  ASSERT_TRUE(test_server_.Start());

  AsyncLoggingUrlRequestDelegate request_delegate(
      AsyncLoggingUrlRequestDelegate::NO_CANCEL);
  TestURLRequestContext context(true);
  context.set_network_delegate(NULL);
  context.set_net_log(&net_log_);
  context.Init();

  {
    // A chunked response with delays between chunks is used to make sure that
    // attempts by the URLRequest delegate to log information while reading the
    // body are ignored.  Since they are ignored, this test is robust against
    // the possibility of multiple reads being combined in the unlikely event
    // that it occurs.
    URLRequest r(test_server_.GetURL("chunked?waitBetweenChunks=20"),
                 DEFAULT_PRIORITY,
                 &request_delegate,
                 &context);
    LoadStateWithParam load_state = r.GetLoadState();
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  }

  CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);

  size_t log_position = 0;

  log_position = AsyncDelegateLogger::ExpectBeforeNetworkEvents(
      entries, log_position);

  // The delegate info should only have been logged on header complete.  Other
  // times it should silently be ignored.
  log_position =
      ExpectLogContainsSomewhereAfter(entries,
                                      log_position + 1,
                                      NetLog::TYPE_URL_REQUEST_DELEGATE,
                                      NetLog::PHASE_BEGIN);

  log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                        log_position + 1);

  ASSERT_LT(log_position, entries.size());
  EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
  EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_URL_REQUEST_DELEGATE));
}

// Tests handling of delegate info from a URLRequest::Delegate in the case of
// an HTTP redirect.
TEST_F(URLRequestTestHTTP, URLRequestDelegateInfoOnRedirect) {
  ASSERT_TRUE(test_server_.Start());

  AsyncLoggingUrlRequestDelegate request_delegate(
      AsyncLoggingUrlRequestDelegate::NO_CANCEL);
  TestURLRequestContext context(true);
  context.set_network_delegate(NULL);
  context.set_net_log(&net_log_);
  context.Init();

  {
    URLRequest r(test_server_.GetURL("server-redirect?simple.html"),
                 DEFAULT_PRIORITY,
                 &request_delegate,
                 &context);
    LoadStateWithParam load_state = r.GetLoadState();
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  }

  CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);

  // Delegate info should only have been logged in OnReceivedRedirect and
  // OnResponseStarted.
  size_t log_position = 0;
  for (int i = 0; i < 2; ++i) {
    if (i == 0) {
      log_position = AsyncDelegateLogger::ExpectBeforeNetworkEvents(
                         entries, log_position) + 1;
    }

    log_position = ExpectLogContainsSomewhereAfter(
            entries,
            log_position,
            NetLog::TYPE_URL_REQUEST_DELEGATE,
            NetLog::PHASE_BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
    EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1, NetLog::TYPE_URL_REQUEST_DELEGATE));
}

// Tests handling of delegate info from a URLRequest::Delegate in the case of
// an HTTP redirect, with cancellation at various points.
TEST_F(URLRequestTestHTTP, URLRequestDelegateOnRedirectCancelled) {
  ASSERT_TRUE(test_server_.Start());

  const AsyncLoggingUrlRequestDelegate::CancelStage kCancelStages[] = {
    AsyncLoggingUrlRequestDelegate::CANCEL_ON_RECEIVED_REDIRECT,
    AsyncLoggingUrlRequestDelegate::CANCEL_ON_RESPONSE_STARTED,
    AsyncLoggingUrlRequestDelegate::CANCEL_ON_READ_COMPLETED,
  };

  for (size_t test_case = 0; test_case < arraysize(kCancelStages);
       ++test_case) {
    AsyncLoggingUrlRequestDelegate request_delegate(kCancelStages[test_case]);
    TestURLRequestContext context(true);
    CapturingNetLog net_log;
    context.set_network_delegate(NULL);
    context.set_net_log(&net_log);
    context.Init();

    {
      URLRequest r(test_server_.GetURL("server-redirect?simple.html"),
                   DEFAULT_PRIORITY,
                   &request_delegate,
                   &context);
      LoadStateWithParam load_state = r.GetLoadState();
      r.Start();
      base::RunLoop().Run();
      EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    }

    CapturingNetLog::CapturedEntryList entries;
    net_log.GetEntries(&entries);

    // Delegate info is always logged in both OnReceivedRedirect and
    // OnResponseStarted.  In the CANCEL_ON_RECEIVED_REDIRECT, the
    // OnResponseStarted delegate call is after cancellation, but logging is
    // still currently supported in that call.
    size_t log_position = 0;
    for (int i = 0; i < 2; ++i) {
      if (i == 0) {
        log_position = AsyncDelegateLogger::ExpectBeforeNetworkEvents(
                           entries, log_position) + 1;
      }

      log_position = ExpectLogContainsSomewhereAfter(
              entries,
              log_position,
              NetLog::TYPE_URL_REQUEST_DELEGATE,
              NetLog::PHASE_BEGIN);

      log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                            log_position + 1);

      ASSERT_LT(log_position, entries.size());
      EXPECT_EQ(NetLog::TYPE_URL_REQUEST_DELEGATE, entries[log_position].type);
      EXPECT_EQ(NetLog::PHASE_END, entries[log_position].phase);
    }

    EXPECT_FALSE(LogContainsEntryWithTypeAfter(
        entries, log_position + 1, NetLog::TYPE_DELEGATE_INFO));
    EXPECT_FALSE(LogContainsEntryWithTypeAfter(
        entries, log_position + 1, NetLog::TYPE_URL_REQUEST_DELEGATE));
  }
}

namespace {

const char kExtraHeader[] = "Allow-Snafu";
const char kExtraValue[] = "fubar";

class RedirectWithAdditionalHeadersDelegate : public TestDelegate {
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE {
    TestDelegate::OnReceivedRedirect(request, new_url, defer_redirect);
    request->SetExtraRequestHeaderByName(kExtraHeader, kExtraValue, false);
  }
};

}  // namespace

TEST_F(URLRequestTestHTTP, RedirectWithAdditionalHeadersTest) {
  ASSERT_TRUE(test_server_.Start());

  GURL destination_url = test_server_.GetURL(
      "echoheader?" + std::string(kExtraHeader));
  GURL original_url = test_server_.GetURL(
      "server-redirect?" + destination_url.spec());
  RedirectWithAdditionalHeadersDelegate d;
  URLRequest req(original_url, DEFAULT_PRIORITY, &d, &default_context_);
  req.Start();
  base::RunLoop().Run();

  std::string value;
  const HttpRequestHeaders& headers = req.extra_request_headers();
  EXPECT_TRUE(headers.GetHeader(kExtraHeader, &value));
  EXPECT_EQ(kExtraValue, value);
  EXPECT_FALSE(req.is_pending());
  EXPECT_FALSE(req.is_redirecting());
  EXPECT_EQ(kExtraValue, d.data_received());
}

namespace {

const char kExtraHeaderToRemove[] = "To-Be-Removed";

class RedirectWithHeaderRemovalDelegate : public TestDelegate {
  virtual void OnReceivedRedirect(net::URLRequest* request,
                          const GURL& new_url,
                          bool* defer_redirect) OVERRIDE {
    TestDelegate::OnReceivedRedirect(request, new_url, defer_redirect);
    request->RemoveRequestHeaderByName(kExtraHeaderToRemove);
  }
};

}  // namespace

TEST_F(URLRequestTestHTTP, RedirectWithHeaderRemovalTest) {
  ASSERT_TRUE(test_server_.Start());

  GURL destination_url = test_server_.GetURL(
      "echoheader?" + std::string(kExtraHeaderToRemove));
  GURL original_url = test_server_.GetURL(
      "server-redirect?" + destination_url.spec());
  RedirectWithHeaderRemovalDelegate d;
  URLRequest req(original_url, DEFAULT_PRIORITY, &d, &default_context_);
  req.SetExtraRequestHeaderByName(kExtraHeaderToRemove, "dummy", false);
  req.Start();
  base::RunLoop().Run();

  std::string value;
  const HttpRequestHeaders& headers = req.extra_request_headers();
  EXPECT_FALSE(headers.GetHeader(kExtraHeaderToRemove, &value));
  EXPECT_FALSE(req.is_pending());
  EXPECT_FALSE(req.is_redirecting());
  EXPECT_EQ("None", d.data_received());
}

TEST_F(URLRequestTestHTTP, CancelTest) {
  TestDelegate d;
  {
    URLRequest r(GURL("http://www.google.com/"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    r.Cancel();

    base::RunLoop().Run();

    // We expect to receive OnResponseStarted even though the request has been
    // cancelled.
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest2) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    d.set_cancel_in_response_started(true);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest3) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    d.set_cancel_in_received_data(true);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    // There is no guarantee about how much data was received
    // before the cancel was issued.  It could have been 0 bytes,
    // or it could have been all the bytes.
    // EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest4) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    // The request will be implicitly canceled when it is destroyed. The
    // test delegate must not post a quit message when this happens because
    // this test doesn't actually have a message loop. The quit message would
    // get put on this thread's message queue and the next test would exit
    // early, causing problems.
    d.set_quit_on_complete(false);
  }
  // expect things to just cleanup properly.

  // we won't actually get a received reponse here because we've never run the
  // message loop
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_EQ(0, d.bytes_received());
}

TEST_F(URLRequestTestHTTP, CancelTest5) {
  ASSERT_TRUE(test_server_.Start());

  // populate cache
  {
    TestDelegate d;
    URLRequest r(test_server_.GetURL("cachetime"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    base::RunLoop().Run();
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  }

  // cancel read from cache (see bug 990242)
  {
    TestDelegate d;
    URLRequest r(test_server_.GetURL("cachetime"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    r.Cancel();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
}

TEST_F(URLRequestTestHTTP, PostTest) {
  ASSERT_TRUE(test_server_.Start());
  HTTPUploadDataOperationTest("POST");
}

TEST_F(URLRequestTestHTTP, PutTest) {
  ASSERT_TRUE(test_server_.Start());
  HTTPUploadDataOperationTest("PUT");
}

TEST_F(URLRequestTestHTTP, PostEmptyTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURL("echo"), DEFAULT_PRIORITY, &d, &default_context_);
    r.set_method("POST");

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    ASSERT_EQ(1, d.response_started_count())
        << "request failed: " << r.status().status()
        << ", error: " << r.status().error();

    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_TRUE(d.data_received().empty());
  }
}

TEST_F(URLRequestTestHTTP, PostFileTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURL("echo"), DEFAULT_PRIORITY, &d, &default_context_);
    r.set_method("POST");

    base::FilePath dir;
    PathService::Get(base::DIR_EXE, &dir);
    base::SetCurrentDirectory(dir);

    ScopedVector<UploadElementReader> element_readers;

    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(FILE_PATH_LITERAL("net"));
    path = path.Append(FILE_PATH_LITERAL("data"));
    path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));
    element_readers.push_back(
        new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                    path,
                                    0,
                                    kuint64max,
                                    base::Time()));
    r.set_upload(make_scoped_ptr(
        new UploadDataStream(element_readers.Pass(), 0)));

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 size = 0;
    ASSERT_EQ(true, base::GetFileSize(path, &size));
    scoped_ptr<char[]> buf(new char[size]);

    ASSERT_EQ(size, base::ReadFile(path, buf.get(), size));

    ASSERT_EQ(1, d.response_started_count())
        << "request failed: " << r.status().status()
        << ", error: " << r.status().error();

    EXPECT_FALSE(d.received_data_before_response());

    EXPECT_EQ(size, d.bytes_received());
    EXPECT_EQ(std::string(&buf[0], size), d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, PostUnreadableFileTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL("echo"), DEFAULT_PRIORITY,
                 &d, &default_context_);
    r.set_method("POST");

    ScopedVector<UploadElementReader> element_readers;

    element_readers.push_back(new UploadFileElementReader(
        base::MessageLoopProxy::current().get(),
        base::FilePath(FILE_PATH_LITERAL(
            "c:\\path\\to\\non\\existant\\file.randomness.12345")),
        0,
        kuint64max,
        base::Time()));
    r.set_upload(make_scoped_ptr(
        new UploadDataStream(element_readers.Pass(), 0)));

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_TRUE(d.request_failed());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_FILE_NOT_FOUND, r.status().error());
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataBeforeStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURL("echo"), DEFAULT_PRIORITY, &d, &default_context_);
    r.EnableChunkedUpload();
    r.set_method("POST");
    AddChunksToUpload(&r);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    VerifyReceivedDataMatchesChunks(&r, &d);
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataJustAfterStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURL("echo"), DEFAULT_PRIORITY, &d, &default_context_);
    r.EnableChunkedUpload();
    r.set_method("POST");
    r.Start();
    EXPECT_TRUE(r.is_pending());
    AddChunksToUpload(&r);
    base::RunLoop().Run();

    VerifyReceivedDataMatchesChunks(&r, &d);
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataAfterStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURL("echo"), DEFAULT_PRIORITY, &d, &default_context_);
    r.EnableChunkedUpload();
    r.set_method("POST");
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().RunUntilIdle();
    AddChunksToUpload(&r);
    base::RunLoop().Run();

    VerifyReceivedDataMatchesChunks(&r, &d);
  }
}

TEST_F(URLRequestTestHTTP, ResponseHeadersTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("files/with-headers.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.Start();
  base::RunLoop().Run();

  const HttpResponseHeaders* headers = req.response_headers();

  // Simple sanity check that response_info() accesses the same data.
  EXPECT_EQ(headers, req.response_info().headers.get());

  std::string header;
  EXPECT_TRUE(headers->GetNormalizedHeader("cache-control", &header));
  EXPECT_EQ("private", header);

  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("content-type", &header));
  EXPECT_EQ("text/html; charset=ISO-8859-1", header);

  // The response has two "X-Multiple-Entries" headers.
  // This verfies our output has them concatenated together.
  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("x-multiple-entries", &header));
  EXPECT_EQ("a, b", header);
}

TEST_F(URLRequestTestHTTP, ProcessSTS) {
  SpawnedTestServer::SSLOptions ssl_options;
  SpawnedTestServer https_test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(https_test_server.Start());

  TestDelegate d;
  URLRequest request(https_test_server.GetURL("files/hsts-headers.html"),
                     DEFAULT_PRIORITY,
                     &d,
                     &default_context_);
  request.Start();
  base::RunLoop().Run();

  TransportSecurityState* security_state =
      default_context_.transport_security_state();
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(security_state->GetDynamicDomainState(
      SpawnedTestServer::kLocalhost, &domain_state));
  EXPECT_EQ(TransportSecurityState::DomainState::MODE_FORCE_HTTPS,
            domain_state.sts.upgrade_mode);
  EXPECT_TRUE(domain_state.sts.include_subdomains);
  EXPECT_FALSE(domain_state.pkp.include_subdomains);
#if defined(OS_ANDROID)
  // Android's CertVerifyProc does not (yet) handle pins.
#else
  EXPECT_FALSE(domain_state.HasPublicKeyPins());
#endif
}

// Android's CertVerifyProc does not (yet) handle pins. Therefore, it will
// reject HPKP headers, and a test setting only HPKP headers will fail (no
// DomainState present because header rejected).
#if defined(OS_ANDROID)
#define MAYBE_ProcessPKP DISABLED_ProcessPKP
#else
#define MAYBE_ProcessPKP ProcessPKP
#endif

// Tests that enabling HPKP on a domain does not affect the HSTS
// validity/expiration.
TEST_F(URLRequestTestHTTP, MAYBE_ProcessPKP) {
  SpawnedTestServer::SSLOptions ssl_options;
  SpawnedTestServer https_test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(https_test_server.Start());

  TestDelegate d;
  URLRequest request(https_test_server.GetURL("files/hpkp-headers.html"),
                     DEFAULT_PRIORITY,
                     &d,
                     &default_context_);
  request.Start();
  base::RunLoop().Run();

  TransportSecurityState* security_state =
      default_context_.transport_security_state();
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(security_state->GetDynamicDomainState(
      SpawnedTestServer::kLocalhost, &domain_state));
  EXPECT_EQ(TransportSecurityState::DomainState::MODE_DEFAULT,
            domain_state.sts.upgrade_mode);
  EXPECT_FALSE(domain_state.sts.include_subdomains);
  EXPECT_FALSE(domain_state.pkp.include_subdomains);
  EXPECT_TRUE(domain_state.HasPublicKeyPins());
  EXPECT_NE(domain_state.sts.expiry, domain_state.pkp.expiry);
}

TEST_F(URLRequestTestHTTP, ProcessSTSOnce) {
  SpawnedTestServer::SSLOptions ssl_options;
  SpawnedTestServer https_test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(https_test_server.Start());

  TestDelegate d;
  URLRequest request(
      https_test_server.GetURL("files/hsts-multiple-headers.html"),
      DEFAULT_PRIORITY,
      &d,
      &default_context_);
  request.Start();
  base::RunLoop().Run();

  // We should have set parameters from the first header, not the second.
  TransportSecurityState* security_state =
      default_context_.transport_security_state();
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(security_state->GetDynamicDomainState(
      SpawnedTestServer::kLocalhost, &domain_state));
  EXPECT_EQ(TransportSecurityState::DomainState::MODE_FORCE_HTTPS,
            domain_state.sts.upgrade_mode);
  EXPECT_FALSE(domain_state.sts.include_subdomains);
  EXPECT_FALSE(domain_state.pkp.include_subdomains);
}

TEST_F(URLRequestTestHTTP, ProcessSTSAndPKP) {
  SpawnedTestServer::SSLOptions ssl_options;
  SpawnedTestServer https_test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(https_test_server.Start());

  TestDelegate d;
  URLRequest request(
      https_test_server.GetURL("files/hsts-and-hpkp-headers.html"),
      DEFAULT_PRIORITY,
      &d,
      &default_context_);
  request.Start();
  base::RunLoop().Run();

  // We should have set parameters from the first header, not the second.
  TransportSecurityState* security_state =
      default_context_.transport_security_state();
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(security_state->GetDynamicDomainState(
      SpawnedTestServer::kLocalhost, &domain_state));
  EXPECT_EQ(TransportSecurityState::DomainState::MODE_FORCE_HTTPS,
            domain_state.sts.upgrade_mode);
#if defined(OS_ANDROID)
  // Android's CertVerifyProc does not (yet) handle pins.
#else
  EXPECT_TRUE(domain_state.HasPublicKeyPins());
#endif
  EXPECT_NE(domain_state.sts.expiry, domain_state.pkp.expiry);

  // Even though there is an HSTS header asserting includeSubdomains, it is
  // the *second* such header, and we MUST process only the first.
  EXPECT_FALSE(domain_state.sts.include_subdomains);
  // includeSubdomains does not occur in the test HPKP header.
  EXPECT_FALSE(domain_state.pkp.include_subdomains);
}

// Tests that when multiple HPKP headers are present, asserting different
// policies, that only the first such policy is processed.
TEST_F(URLRequestTestHTTP, ProcessSTSAndPKP2) {
  SpawnedTestServer::SSLOptions ssl_options;
  SpawnedTestServer https_test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(https_test_server.Start());

  TestDelegate d;
  URLRequest request(
      https_test_server.GetURL("files/hsts-and-hpkp-headers2.html"),
      DEFAULT_PRIORITY,
      &d,
      &default_context_);
  request.Start();
  base::RunLoop().Run();

  TransportSecurityState* security_state =
      default_context_.transport_security_state();
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(security_state->GetDynamicDomainState(
      SpawnedTestServer::kLocalhost, &domain_state));
  EXPECT_EQ(TransportSecurityState::DomainState::MODE_FORCE_HTTPS,
            domain_state.sts.upgrade_mode);
#if defined(OS_ANDROID)
  // Android's CertVerifyProc does not (yet) handle pins.
#else
  EXPECT_TRUE(domain_state.HasPublicKeyPins());
#endif
  EXPECT_NE(domain_state.sts.expiry, domain_state.pkp.expiry);

  EXPECT_TRUE(domain_state.sts.include_subdomains);
  EXPECT_FALSE(domain_state.pkp.include_subdomains);
}

TEST_F(URLRequestTestHTTP, ContentTypeNormalizationTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("files/content-type-normalization.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.Start();
  base::RunLoop().Run();

  std::string mime_type;
  req.GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  std::string charset;
  req.GetCharset(&charset);
  EXPECT_EQ("utf-8", charset);
  req.Cancel();
}

TEST_F(URLRequestTestHTTP, ProtocolHandlerAndFactoryRestrictDataRedirects) {
  // Test URLRequestJobFactory::ProtocolHandler::IsSafeRedirectTarget().
  GURL data_url("data:,foo");
  DataProtocolHandler data_protocol_handler;
  EXPECT_FALSE(data_protocol_handler.IsSafeRedirectTarget(data_url));

  // Test URLRequestJobFactoryImpl::IsSafeRedirectTarget().
  EXPECT_FALSE(job_factory_.IsSafeRedirectTarget(data_url));
}

#if !defined(DISABLE_FILE_SUPPORT)
TEST_F(URLRequestTestHTTP, ProtocolHandlerAndFactoryRestrictFileRedirects) {
  // Test URLRequestJobFactory::ProtocolHandler::IsSafeRedirectTarget().
  GURL file_url("file:///foo.txt");
  FileProtocolHandler file_protocol_handler(base::MessageLoopProxy::current());
  EXPECT_FALSE(file_protocol_handler.IsSafeRedirectTarget(file_url));

  // Test URLRequestJobFactoryImpl::IsSafeRedirectTarget().
  EXPECT_FALSE(job_factory_.IsSafeRedirectTarget(file_url));
}

TEST_F(URLRequestTestHTTP, RestrictFileRedirects) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("files/redirect-to-file.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_UNSAFE_REDIRECT, req.status().error());
}
#endif  // !defined(DISABLE_FILE_SUPPORT)

TEST_F(URLRequestTestHTTP, RestrictDataRedirects) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("files/redirect-to-data.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.Start();
  base::MessageLoop::current()->Run();

  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_UNSAFE_REDIRECT, req.status().error());
}

TEST_F(URLRequestTestHTTP, RedirectToInvalidURL) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("files/redirect-to-invalid-url.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_INVALID_URL, req.status().error());
}

// Make sure redirects are cached, despite not reading their bodies.
TEST_F(URLRequestTestHTTP, CacheRedirect) {
  ASSERT_TRUE(test_server_.Start());
  GURL redirect_url =
      test_server_.GetURL("files/redirect302-to-echo-cacheable");

  {
    TestDelegate d;
    URLRequest req(redirect_url, DEFAULT_PRIORITY, &d, &default_context_);
    req.Start();
    base::RunLoop().Run();
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(test_server_.GetURL("echo"), req.url());
  }

  {
    TestDelegate d;
    d.set_quit_on_redirect(true);
    URLRequest req(redirect_url, DEFAULT_PRIORITY, &d, &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(0, d.response_started_count());
    EXPECT_TRUE(req.was_cached());

    req.FollowDeferredRedirect();
    base::RunLoop().Run();
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
    EXPECT_EQ(test_server_.GetURL("echo"), req.url());
  }
}

// Make sure a request isn't cached when a NetworkDelegate forces a redirect
// when the headers are read, since the body won't have been read.
TEST_F(URLRequestTestHTTP, NoCacheOnNetworkDelegateRedirect) {
  ASSERT_TRUE(test_server_.Start());
  // URL that is normally cached.
  GURL initial_url = test_server_.GetURL("cachetime");

  {
    // Set up the TestNetworkDelegate tp force a redirect.
    GURL redirect_to_url = test_server_.GetURL("echo");
    default_network_delegate_.set_redirect_on_headers_received_url(
        redirect_to_url);

    TestDelegate d;
    URLRequest req(initial_url, DEFAULT_PRIORITY, &d, &default_context_);
    req.Start();
    base::RunLoop().Run();
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(redirect_to_url, req.url());
  }

  {
    TestDelegate d;
    URLRequest req(initial_url, DEFAULT_PRIORITY, &d, &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
    EXPECT_FALSE(req.was_cached());
    EXPECT_EQ(0, d.received_redirect_count());
    EXPECT_EQ(initial_url, req.url());
  }
}

// Tests that redirection to an unsafe URL is allowed when it has been marked as
// safe.
TEST_F(URLRequestTestHTTP, UnsafeRedirectToWhitelistedUnsafeURL) {
  ASSERT_TRUE(test_server_.Start());

  GURL unsafe_url("data:text/html,this-is-considered-an-unsafe-url");
  default_network_delegate_.set_redirect_on_headers_received_url(unsafe_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(unsafe_url);

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL("whatever"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());

    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(net::OK, r.status().error());
    EXPECT_EQ(unsafe_url, r.url());
    EXPECT_EQ("this-is-considered-an-unsafe-url", d.data_received());
  }
}

// Tests that a redirect to a different unsafe URL is blocked, even after adding
// some other URL to the whitelist.
TEST_F(URLRequestTestHTTP, UnsafeRedirectToDifferentUnsafeURL) {
  ASSERT_TRUE(test_server_.Start());

  GURL unsafe_url("data:text/html,something");
  GURL different_unsafe_url("data:text/html,something-else");
  default_network_delegate_.set_redirect_on_headers_received_url(unsafe_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(
      different_unsafe_url);

  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL("whatever"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_UNSAFE_REDIRECT, r.status().error());
  }
}

// Redirects from an URL with fragment to an unsafe URL with fragment should
// be allowed, and the reference fragment of the target URL should be preserved.
TEST_F(URLRequestTestHTTP, UnsafeRedirectWithDifferentReferenceFragment) {
  ASSERT_TRUE(test_server_.Start());

  GURL original_url(test_server_.GetURL("original#fragment1"));
  GURL unsafe_url("data:,url-marked-safe-and-used-in-redirect#fragment2");
  GURL expected_url("data:,url-marked-safe-and-used-in-redirect#fragment2");

  default_network_delegate_.set_redirect_on_headers_received_url(unsafe_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(unsafe_url);

  TestDelegate d;
  {
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(net::OK, r.status().error());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(expected_url, r.url());
  }
}

// When a delegate has specified a safe redirect URL, but it does not match the
// redirect target, then do not prevent the reference fragment from being added.
TEST_F(URLRequestTestHTTP, RedirectWithReferenceFragmentAndUnrelatedUnsafeUrl) {
  ASSERT_TRUE(test_server_.Start());

  GURL original_url(test_server_.GetURL("original#expected-fragment"));
  GURL unsafe_url("data:text/html,this-url-does-not-match-redirect-url");
  GURL redirect_url(test_server_.GetURL("target"));
  GURL expected_redirect_url(test_server_.GetURL("target#expected-fragment"));

  default_network_delegate_.set_redirect_on_headers_received_url(redirect_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(unsafe_url);

  TestDelegate d;
  {
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(net::OK, r.status().error());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(expected_redirect_url, r.url());
  }
}

// When a delegate has specified a safe redirect URL, assume that the redirect
// URL should not be changed. In particular, the reference fragment should not
// be modified.
TEST_F(URLRequestTestHTTP, RedirectWithReferenceFragment) {
  ASSERT_TRUE(test_server_.Start());

  GURL original_url(test_server_.GetURL("original#should-not-be-appended"));
  GURL redirect_url("data:text/html,expect-no-reference-fragment");

  default_network_delegate_.set_redirect_on_headers_received_url(redirect_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(redirect_url);

  TestDelegate d;
  {
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(net::OK, r.status().error());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(redirect_url, r.url());
  }
}

// When a URLRequestRedirectJob is created, the redirection must be followed and
// the reference fragment of the target URL must not be modified.
TEST_F(URLRequestTestHTTP, RedirectJobWithReferenceFragment) {
  ASSERT_TRUE(test_server_.Start());

  GURL original_url(test_server_.GetURL("original#should-not-be-appended"));
  GURL redirect_url(test_server_.GetURL("echo"));

  TestDelegate d;
  URLRequest r(original_url, DEFAULT_PRIORITY, &d, &default_context_);

  URLRequestRedirectJob* job = new URLRequestRedirectJob(
      &r, &default_network_delegate_, redirect_url,
      URLRequestRedirectJob::REDIRECT_302_FOUND, "Very Good Reason");
  AddTestInterceptor()->set_main_intercept_job(job);

  r.Start();
  base::RunLoop().Run();

  EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  EXPECT_EQ(net::OK, r.status().error());
  EXPECT_EQ(original_url, r.original_url());
  EXPECT_EQ(redirect_url, r.url());
}

TEST_F(URLRequestTestHTTP, NoUserPassInReferrer) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Referer"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.SetReferrer("http://user:pass@foo.com/");
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(std::string("http://foo.com/"), d.data_received());
}

TEST_F(URLRequestTestHTTP, NoFragmentInReferrer) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Referer"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.SetReferrer("http://foo.com/test#fragment");
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(std::string("http://foo.com/test"), d.data_received());
}

TEST_F(URLRequestTestHTTP, EmptyReferrerAfterValidReferrer) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Referer"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.SetReferrer("http://foo.com/test#fragment");
  req.SetReferrer("");
  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ(std::string("None"), d.data_received());
}

// Defer network start and then resume, checking that the request was a success
// and bytes were received.
TEST_F(URLRequestTestHTTP, DeferredBeforeNetworkStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_network_start(true);
    GURL test_url(test_server_.GetURL("echo"));
    URLRequest req(test_url, DEFAULT_PRIORITY, &d, &default_context_);

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_before_network_start_count());
    EXPECT_EQ(0, d.response_started_count());

    req.ResumeNetworkStart();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
  }
}

// Check that OnBeforeNetworkStart is only called once even if there is a
// redirect.
TEST_F(URLRequestTestHTTP, BeforeNetworkStartCalledOnce) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_redirect(true);
    d.set_quit_on_network_start(true);
    URLRequest req(test_server_.GetURL("server-redirect?echo"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_before_network_start_count());
    EXPECT_EQ(0, d.response_started_count());
    EXPECT_EQ(0, d.received_redirect_count());

    req.ResumeNetworkStart();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_redirect_count());
    req.FollowDeferredRedirect();
    base::RunLoop().Run();

    // Check that the redirect's new network transaction does not get propagated
    // to a second OnBeforeNetworkStart() notification.
    EXPECT_EQ(1, d.received_before_network_start_count());

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
  }
}

// Cancel the request after learning that the request would use the network.
TEST_F(URLRequestTestHTTP, CancelOnBeforeNetworkStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_network_start(true);
    GURL test_url(test_server_.GetURL("echo"));
    URLRequest req(test_url, DEFAULT_PRIORITY, &d, &default_context_);

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_before_network_start_count());
    EXPECT_EQ(0, d.response_started_count());

    req.Cancel();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
  }
}

TEST_F(URLRequestTestHTTP, CancelRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_cancel_in_received_redirect(true);
    URLRequest req(test_server_.GetURL("files/redirect-test.html"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_redirect(true);
    GURL test_url(test_server_.GetURL("files/redirect-test.html"));
    URLRequest req(test_url, DEFAULT_PRIORITY, &d, &default_context_);

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_redirect_count());

    req.FollowDeferredRedirect();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());

    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(FILE_PATH_LITERAL("net"));
    path = path.Append(FILE_PATH_LITERAL("data"));
    path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));

    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect_GetFullRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_redirect(true);
    GURL test_url(test_server_.GetURL("files/redirect-test.html"));
    URLRequest req(test_url, DEFAULT_PRIORITY, &d, &default_context_);

    EXPECT_FALSE(d.have_full_request_headers());

    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_TRUE(d.have_full_request_headers());
    CheckFullRequestHeaders(d.full_request_headers(), test_url);
    d.ClearFullRequestHeaders();

    req.FollowDeferredRedirect();
    base::RunLoop().Run();

    GURL target_url(test_server_.GetURL("files/with-headers.html"));
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_TRUE(d.have_full_request_headers());
    CheckFullRequestHeaders(d.full_request_headers(), target_url);
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());

    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(FILE_PATH_LITERAL("net"));
    path = path.Append(FILE_PATH_LITERAL("data"));
    path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));

    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, CancelDeferredRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_redirect(true);
    URLRequest req(test_server_.GetURL("files/redirect-test.html"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    req.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_redirect_count());

    req.Cancel();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
  }
}

TEST_F(URLRequestTestHTTP, VaryHeader) {
  ASSERT_TRUE(test_server_.Start());

  // Populate the cache.
  {
    TestDelegate d;
    URLRequest req(test_server_.GetURL("echoheadercache?foo"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "1");
    req.SetExtraRequestHeaders(headers);
    req.Start();
    base::RunLoop().Run();

    LoadTimingInfo load_timing_info;
    req.GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
  }

  // Expect a cache hit.
  {
    TestDelegate d;
    URLRequest req(test_server_.GetURL("echoheadercache?foo"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "1");
    req.SetExtraRequestHeaders(headers);
    req.Start();
    base::RunLoop().Run();

    EXPECT_TRUE(req.was_cached());

    LoadTimingInfo load_timing_info;
    req.GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingCacheHitNoNetwork(load_timing_info);
  }

  // Expect a cache miss.
  {
    TestDelegate d;
    URLRequest req(test_server_.GetURL("echoheadercache?foo"),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "2");
    req.SetExtraRequestHeaders(headers);
    req.Start();
    base::RunLoop().Run();

    EXPECT_FALSE(req.was_cached());

    LoadTimingInfo load_timing_info;
    req.GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
  }
}

TEST_F(URLRequestTestHTTP, BasicAuth) {
  ASSERT_TRUE(test_server_.Start());

  // populate the cache
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(test_server_.GetURL("auth-basic"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();

    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);
  }

  // repeat request with end-to-end validation.  since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  in which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(test_server_.GetURL("auth-basic"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.SetLoadFlags(LOAD_VALIDATE_CACHE);
    r.Start();

    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Should be the same cached document.
    EXPECT_TRUE(r.was_cached());
  }
}

// Check that Set-Cookie headers in 401 responses are respected.
// http://crbug.com/6450
TEST_F(URLRequestTestHTTP, BasicAuthWithCookies) {
  ASSERT_TRUE(test_server_.Start());

  GURL url_requiring_auth =
      test_server_.GetURL("auth-basic?set-cookie-if-challenged");

  // Request a page that will give a 401 containing a Set-Cookie header.
  // Verify that when the transaction is restarted, it includes the new cookie.
  {
    TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
    TestURLRequestContext context(true);
    context.set_network_delegate(&network_delegate);
    context.Init();

    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(url_requiring_auth, DEFAULT_PRIORITY, &d, &context);
    r.Start();

    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Make sure we sent the cookie in the restarted transaction.
    EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true")
        != std::string::npos);
  }

  // Same test as above, except this time the restart is initiated earlier
  // (without user intervention since identity is embedded in the URL).
  {
    TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
    TestURLRequestContext context(true);
    context.set_network_delegate(&network_delegate);
    context.Init();

    TestDelegate d;

    GURL::Replacements replacements;
    std::string username("user2");
    std::string password("secret");
    replacements.SetUsernameStr(username);
    replacements.SetPasswordStr(password);
    GURL url_with_identity = url_requiring_auth.ReplaceComponents(replacements);

    URLRequest r(url_with_identity, DEFAULT_PRIORITY, &d, &context);
    r.Start();

    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("user2/secret") != std::string::npos);

    // Make sure we sent the cookie in the restarted transaction.
    EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true")
        != std::string::npos);
  }
}

// Tests that load timing works as expected with auth and the cache.
TEST_F(URLRequestTestHTTP, BasicAuthLoadTiming) {
  ASSERT_TRUE(test_server_.Start());

  // populate the cache
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(test_server_.GetURL("auth-basic"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();

    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    LoadTimingInfo load_timing_info_before_auth;
    EXPECT_TRUE(default_network_delegate_.GetLoadTimingInfoBeforeAuth(
        &load_timing_info_before_auth));
    TestLoadTimingNotReused(load_timing_info_before_auth,
                            CONNECT_TIMING_HAS_DNS_TIMES);

    LoadTimingInfo load_timing_info;
    r.GetLoadTimingInfo(&load_timing_info);
    // The test server does not support keep alive sockets, so the second
    // request with auth should use a new socket.
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
    EXPECT_NE(load_timing_info_before_auth.socket_log_id,
              load_timing_info.socket_log_id);
    EXPECT_LE(load_timing_info_before_auth.receive_headers_end,
              load_timing_info.connect_timing.connect_start);
  }

  // Repeat request with end-to-end validation.  Since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  In which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(test_server_.GetURL("auth-basic"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.SetLoadFlags(LOAD_VALIDATE_CACHE);
    r.Start();

    base::RunLoop().Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Should be the same cached document.
    EXPECT_TRUE(r.was_cached());

    // Since there was a request that went over the wire, the load timing
    // information should include connection times.
    LoadTimingInfo load_timing_info;
    r.GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
  }
}

// In this test, we do a POST which the server will 302 redirect.
// The subsequent transaction should use GET, and should not send the
// Content-Type header.
// http://code.google.com/p/chromium/issues/detail?id=843
TEST_F(URLRequestTestHTTP, Post302RedirectGet) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  URLRequest req(test_server_.GetURL("files/redirect-to-echoall"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("POST");
  req.set_upload(make_scoped_ptr(CreateSimpleUploadData(kData)));

  // Set headers (some of which are specific to the POST).
  HttpRequestHeaders headers;
  headers.AddHeadersFromString(
    "Content-Type: multipart/form-data; "
    "boundary=----WebKitFormBoundaryAADeAA+NAAWMAAwZ\r\n"
    "Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,"
    "text/plain;q=0.8,image/png,*/*;q=0.5\r\n"
    "Accept-Language: en-US,en\r\n"
    "Accept-Charset: ISO-8859-1,*,utf-8\r\n"
    "Content-Length: 11\r\n"
    "Origin: http://localhost:1337/");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  base::RunLoop().Run();

  std::string mime_type;
  req.GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  const std::string& data = d.data_received();

  // Check that the post-specific headers were stripped:
  EXPECT_FALSE(ContainsString(data, "Content-Length:"));
  EXPECT_FALSE(ContainsString(data, "Content-Type:"));
  EXPECT_FALSE(ContainsString(data, "Origin:"));

  // These extra request headers should not have been stripped.
  EXPECT_TRUE(ContainsString(data, "Accept:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Language:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Charset:"));
}

// The following tests check that we handle mutating the request method for
// HTTP redirects as expected.
// See http://crbug.com/56373 and http://crbug.com/102130.

TEST_F(URLRequestTestHTTP, Redirect301Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect301-to-echo");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect302Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect302-to-echo");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect303Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect303-to-echo");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "GET", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect307Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect307-to-echo");

  HTTPRedirectMethodTest(url, "POST", "POST", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect308Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect308-to-echo");

  HTTPRedirectMethodTest(url, "POST", "POST", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

// Make sure that 308 responses without bodies are not treated as redirects.
// Certain legacy apis that pre-date the response code expect this behavior
// (Like Google Drive).
TEST_F(URLRequestTestHTTP, NoRedirectOn308WithoutLocationHeader) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  const GURL url = test_server_.GetURL("files/308-without-location-header");

  URLRequest request(url, DEFAULT_PRIORITY, &d, &default_context_);

  request.Start();
  base::RunLoop().Run();
  EXPECT_EQ(URLRequestStatus::SUCCESS, request.status().status());
  EXPECT_EQ(OK, request.status().error());
  EXPECT_EQ(0, d.received_redirect_count());
  EXPECT_EQ(308, request.response_headers()->response_code());
  EXPECT_EQ("This is not a redirect.", d.data_received());
}

TEST_F(URLRequestTestHTTP, Redirect302PreserveReferenceFragment) {
  ASSERT_TRUE(test_server_.Start());

  GURL original_url(test_server_.GetURL("files/redirect302-to-echo#fragment"));
  GURL expected_url(test_server_.GetURL("echo#fragment"));

  TestDelegate d;
  {
    URLRequest r(original_url, DEFAULT_PRIORITY, &d, &default_context_);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(net::OK, r.status().error());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(expected_url, r.url());
  }
}

TEST_F(URLRequestTestHTTP, InterceptPost302RedirectGet) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  URLRequest req(test_server_.GetURL("empty.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("POST");
  req.set_upload(make_scoped_ptr(CreateSimpleUploadData(kData)));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::UintToString(arraysize(kData) - 1));
  req.SetExtraRequestHeaders(headers);

  URLRequestRedirectJob* job = new URLRequestRedirectJob(
      &req, &default_network_delegate_, test_server_.GetURL("echo"),
      URLRequestRedirectJob::REDIRECT_302_FOUND, "Very Good Reason");
  AddTestInterceptor()->set_main_intercept_job(job);

  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ("GET", req.method());
}

TEST_F(URLRequestTestHTTP, InterceptPost307RedirectPost) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  URLRequest req(test_server_.GetURL("empty.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.set_method("POST");
  req.set_upload(make_scoped_ptr(CreateSimpleUploadData(kData)));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::UintToString(arraysize(kData) - 1));
  req.SetExtraRequestHeaders(headers);

  URLRequestRedirectJob* job = new URLRequestRedirectJob(
      &req, &default_network_delegate_, test_server_.GetURL("echo"),
      URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
      "Very Good Reason");
  AddTestInterceptor()->set_main_intercept_job(job);

  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ("POST", req.method());
  EXPECT_EQ(kData, d.data_received());
}

// Check that default A-L header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptLanguage) {
  ASSERT_TRUE(test_server_.Start());

  StaticHttpUserAgentSettings settings("en", std::string());
  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_http_user_agent_settings(&settings);
  context.Init();

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Accept-Language"),
                 DEFAULT_PRIORITY,
                 &d,
                 &context);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ("en", d.data_received());
}

// Check that an empty A-L header is not sent. http://crbug.com/77365.
TEST_F(URLRequestTestHTTP, EmptyAcceptLanguage) {
  ASSERT_TRUE(test_server_.Start());

  std::string empty_string;  // Avoid most vexing parse on line below.
  StaticHttpUserAgentSettings settings(empty_string, empty_string);
  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();
  // We override the language after initialization because empty entries
  // get overridden by Init().
  context.set_http_user_agent_settings(&settings);

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Accept-Language"),
                 DEFAULT_PRIORITY,
                 &d,
                 &context);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ("None", d.data_received());
}

// Check that if request overrides the A-L header, the default is not appended.
// See http://crbug.com/20894
TEST_F(URLRequestTestHTTP, OverrideAcceptLanguage) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Accept-Language"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptLanguage, "ru");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ(std::string("ru"), d.data_received());
}

// Check that default A-E header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptEncoding) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Accept-Encoding"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  HttpRequestHeaders headers;
  req.SetExtraRequestHeaders(headers);
  req.Start();
  base::RunLoop().Run();
  EXPECT_TRUE(ContainsString(d.data_received(), "gzip"));
}

// Check that if request overrides the A-E header, the default is not appended.
// See http://crbug.com/47381
TEST_F(URLRequestTestHTTP, OverrideAcceptEncoding) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Accept-Encoding"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptEncoding, "identity");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  base::RunLoop().Run();
  EXPECT_FALSE(ContainsString(d.data_received(), "gzip"));
  EXPECT_TRUE(ContainsString(d.data_received(), "identity"));
}

// Check that setting the A-C header sends the proper header.
TEST_F(URLRequestTestHTTP, SetAcceptCharset) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?Accept-Charset"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptCharset, "koi-8r");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ(std::string("koi-8r"), d.data_received());
}

// Check that default User-Agent header is sent.
TEST_F(URLRequestTestHTTP, DefaultUserAgent) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?User-Agent"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ(req.context()->http_user_agent_settings()->GetUserAgent(),
            d.data_received());
}

// Check that if request overrides the User-Agent header,
// the default is not appended.
TEST_F(URLRequestTestHTTP, OverrideUserAgent) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("echoheader?User-Agent"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kUserAgent, "Lynx (textmode)");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  base::RunLoop().Run();
  EXPECT_EQ(std::string("Lynx (textmode)"), d.data_received());
}

// Check that a NULL HttpUserAgentSettings causes the corresponding empty
// User-Agent header to be sent but does not send the Accept-Language and
// Accept-Charset headers.
TEST_F(URLRequestTestHTTP, EmptyHttpUserAgentSettings) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();
  // We override the HttpUserAgentSettings after initialization because empty
  // entries get overridden by Init().
  context.set_http_user_agent_settings(NULL);

  struct {
    const char* request;
    const char* expected_response;
  } tests[] = { { "echoheader?Accept-Language", "None" },
                { "echoheader?Accept-Charset", "None" },
                { "echoheader?User-Agent", "" } };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    TestDelegate d;
    URLRequest req(
        test_server_.GetURL(tests[i].request), DEFAULT_PRIORITY, &d, &context);
    req.Start();
    base::RunLoop().Run();
    EXPECT_EQ(tests[i].expected_response, d.data_received())
        << " Request = \"" << tests[i].request << "\"";
  }
}

// Make sure that URLRequest passes on its priority updates to
// newly-created jobs after the first one.
TEST_F(URLRequestTestHTTP, SetSubsequentJobPriority) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  URLRequest req(test_server_.GetURL("empty.html"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
  EXPECT_EQ(DEFAULT_PRIORITY, req.priority());

  scoped_refptr<URLRequestRedirectJob> redirect_job =
      new URLRequestRedirectJob(
          &req, &default_network_delegate_, test_server_.GetURL("echo"),
          URLRequestRedirectJob::REDIRECT_302_FOUND, "Very Good Reason");
  AddTestInterceptor()->set_main_intercept_job(redirect_job.get());

  req.SetPriority(LOW);
  req.Start();
  EXPECT_TRUE(req.is_pending());

  scoped_refptr<URLRequestTestJob> job =
      new URLRequestTestJob(&req, &default_network_delegate_);
  AddTestInterceptor()->set_main_intercept_job(job.get());

  // Should trigger |job| to be started.
  base::RunLoop().Run();
  EXPECT_EQ(LOW, job->priority());
}

// Check that creating a network request while entering/exiting suspend mode
// fails as it should.  This is the only case where an HttpTransactionFactory
// does not return an HttpTransaction.
TEST_F(URLRequestTestHTTP, NetworkSuspendTest) {
  // Create a new HttpNetworkLayer that thinks it's suspended.
  HttpNetworkSession::Params params;
  params.host_resolver = default_context_.host_resolver();
  params.cert_verifier = default_context_.cert_verifier();
  params.transport_security_state = default_context_.transport_security_state();
  params.proxy_service = default_context_.proxy_service();
  params.ssl_config_service = default_context_.ssl_config_service();
  params.http_auth_handler_factory =
      default_context_.http_auth_handler_factory();
  params.network_delegate = &default_network_delegate_;
  params.http_server_properties = default_context_.http_server_properties();
  scoped_ptr<HttpNetworkLayer> network_layer(
      new HttpNetworkLayer(new HttpNetworkSession(params)));
  network_layer->OnSuspend();

  HttpCache http_cache(network_layer.release(), default_context_.net_log(),
                       HttpCache::DefaultBackend::InMemory(0));

  TestURLRequestContext context(true);
  context.set_http_transaction_factory(&http_cache);
  context.Init();

  TestDelegate d;
  URLRequest req(GURL("http://127.0.0.1/"),
                 DEFAULT_PRIORITY,
                 &d,
                 &context);
  req.Start();
  base::RunLoop().Run();

  EXPECT_TRUE(d.request_failed());
  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, req.status().error());
}

// Check that creating a network request while entering/exiting suspend mode
// fails as it should in the case there is no cache.  This is the only case
// where an HttpTransactionFactory does not return an HttpTransaction.
TEST_F(URLRequestTestHTTP, NetworkSuspendTestNoCache) {
  // Create a new HttpNetworkLayer that thinks it's suspended.
  HttpNetworkSession::Params params;
  params.host_resolver = default_context_.host_resolver();
  params.cert_verifier = default_context_.cert_verifier();
  params.transport_security_state = default_context_.transport_security_state();
  params.proxy_service = default_context_.proxy_service();
  params.ssl_config_service = default_context_.ssl_config_service();
  params.http_auth_handler_factory =
      default_context_.http_auth_handler_factory();
  params.network_delegate = &default_network_delegate_;
  params.http_server_properties = default_context_.http_server_properties();
  HttpNetworkLayer network_layer(new HttpNetworkSession(params));
  network_layer.OnSuspend();

  TestURLRequestContext context(true);
  context.set_http_transaction_factory(&network_layer);
  context.Init();

  TestDelegate d;
  URLRequest req(GURL("http://127.0.0.1/"),
                 DEFAULT_PRIORITY,
                 &d,
                 &context);
  req.Start();
  base::RunLoop().Run();

  EXPECT_TRUE(d.request_failed());
  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, req.status().error());
}

class HTTPSRequestTest : public testing::Test {
 public:
  HTTPSRequestTest() : default_context_(true) {
    default_context_.set_network_delegate(&default_network_delegate_);
    default_context_.Init();
  }
  virtual ~HTTPSRequestTest() {}

 protected:
  TestNetworkDelegate default_network_delegate_;  // Must outlive URLRequest.
  TestURLRequestContext default_context_;
};

TEST_F(HTTPSRequestTest, HTTPSGetTest) {
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  {
    URLRequest r(test_server.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    CheckSSLInfo(r.ssl_info());
    EXPECT_EQ(test_server.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

TEST_F(HTTPSRequestTest, HTTPSMismatchedTest) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  bool err_allowed = true;
  for (int i = 0; i < 2 ; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      URLRequest r(test_server.GetURL(std::string()),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);

      r.Start();
      EXPECT_TRUE(r.is_pending());

      base::RunLoop().Run();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_TRUE(d.have_certificate_errors());
      if (err_allowed) {
        EXPECT_NE(0, d.bytes_received());
        CheckSSLInfo(r.ssl_info());
      } else {
        EXPECT_EQ(0, d.bytes_received());
      }
    }
  }
}

TEST_F(HTTPSRequestTest, HTTPSExpiredTest) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_EXPIRED);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // Iterate from false to true, just so that we do the opposite of the
  // previous test in order to increase test coverage.
  bool err_allowed = false;
  for (int i = 0; i < 2 ; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      URLRequest r(test_server.GetURL(std::string()),
                   DEFAULT_PRIORITY,
                   &d,
                   &default_context_);

      r.Start();
      EXPECT_TRUE(r.is_pending());

      base::RunLoop().Run();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_TRUE(d.have_certificate_errors());
      if (err_allowed) {
        EXPECT_NE(0, d.bytes_received());
        CheckSSLInfo(r.ssl_info());
      } else {
        EXPECT_EQ(0, d.bytes_received());
      }
    }
  }
}

// Tests TLSv1.1 -> TLSv1 fallback. Verifies that we don't fall back more
// than necessary.
TEST_F(HTTPSRequestTest, TLSv1Fallback) {
  // The OpenSSL library in use may not support TLS 1.1.
#if !defined(USE_OPENSSL)
  EXPECT_GT(kDefaultSSLVersionMax, SSL_PROTOCOL_VERSION_TLS1);
#endif
  if (kDefaultSSLVersionMax <= SSL_PROTOCOL_VERSION_TLS1)
    return;

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_TLS1_1;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  TestURLRequestContext context(true);
  context.Init();
  d.set_allow_certificate_errors(true);
  URLRequest r(
      test_server.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);
  r.Start();

  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_NE(0, d.bytes_received());
  EXPECT_EQ(static_cast<int>(SSL_CONNECTION_VERSION_TLS1),
            SSLConnectionStatusToVersion(r.ssl_info().connection_status));
  EXPECT_TRUE(r.ssl_info().connection_status & SSL_CONNECTION_VERSION_FALLBACK);
}

// Tests that we don't fallback with servers that implement TLS_FALLBACK_SCSV.
#if defined(USE_OPENSSL)
TEST_F(HTTPSRequestTest, DISABLED_FallbackSCSV) {
#else
TEST_F(HTTPSRequestTest, FallbackSCSV) {
#endif
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  // Configure HTTPS server to be intolerant of TLS >= 1.0 in order to trigger
  // a version fallback.
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_ALL;
  // Have the server process TLS_FALLBACK_SCSV so that version fallback
  // connections are rejected.
  ssl_options.fallback_scsv_enabled = true;

  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  TestURLRequestContext context(true);
  context.Init();
  d.set_allow_certificate_errors(true);
  URLRequest r(
      test_server.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);
  r.Start();

  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  // ERR_SSL_VERSION_OR_CIPHER_MISMATCH is how the server simulates version
  // intolerance. If the fallback SCSV is processed when the original error
  // that caused the fallback should be returned, which should be
  // ERR_SSL_VERSION_OR_CIPHER_MISMATCH.
  EXPECT_EQ(ERR_SSL_VERSION_OR_CIPHER_MISMATCH, r.status().error());
}

// This tests that a load of www.google.com with a certificate error sets
// the |certificate_errors_are_fatal| flag correctly. This flag will cause
// the interstitial to be fatal.
TEST_F(HTTPSRequestTest, HTTPSPreloadedHSTSTest) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // We require that the URL be www.google.com in order to pick up the
  // preloaded HSTS entries in the TransportSecurityState. This means that we
  // have to use a MockHostResolver in order to direct www.google.com to the
  // testserver. By default, MockHostResolver maps all hosts to 127.0.0.1.

  MockHostResolver host_resolver;
  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_host_resolver(&host_resolver);
  TransportSecurityState transport_security_state;
  context.set_transport_security_state(&transport_security_state);
  context.Init();

  TestDelegate d;
  URLRequest r(GURL(base::StringPrintf("https://www.google.com:%d",
                                       test_server.host_port_pair().port())),
               DEFAULT_PRIORITY,
               &d,
               &context);

  r.Start();
  EXPECT_TRUE(r.is_pending());

  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(d.certificate_errors_are_fatal());
}

// This tests that cached HTTPS page loads do not cause any updates to the
// TransportSecurityState.
TEST_F(HTTPSRequestTest, HTTPSErrorsNoClobberTSSTest) {
  // The actual problem -- CERT_MISMATCHED_NAME in this case -- doesn't
  // matter. It just has to be any error.
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // We require that the URL be www.google.com in order to pick up the static
  // and dynamic STS and PKP entries in the TransportSecurityState. This means
  // that we have to use a MockHostResolver in order to direct www.google.com to
  // the testserver. By default, MockHostResolver maps all hosts to 127.0.0.1.

  MockHostResolver host_resolver;
  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_host_resolver(&host_resolver);
  TransportSecurityState transport_security_state;

  TransportSecurityState::DomainState static_domain_state;
  EXPECT_TRUE(transport_security_state.GetStaticDomainState(
      "www.google.com", true, &static_domain_state));
  context.set_transport_security_state(&transport_security_state);
  context.Init();

  TransportSecurityState::DomainState dynamic_domain_state;
  EXPECT_FALSE(transport_security_state.GetDynamicDomainState(
      "www.google.com", &dynamic_domain_state));

  TestDelegate d;
  URLRequest r(GURL(base::StringPrintf("https://www.google.com:%d",
                                       test_server.host_port_pair().port())),
               DEFAULT_PRIORITY,
               &d,
               &context);

  r.Start();
  EXPECT_TRUE(r.is_pending());

  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(d.certificate_errors_are_fatal());

  // Get a fresh copy of the states, and check that they haven't changed.
  TransportSecurityState::DomainState new_static_domain_state;
  EXPECT_TRUE(transport_security_state.GetStaticDomainState(
      "www.google.com", true, &new_static_domain_state));
  TransportSecurityState::DomainState new_dynamic_domain_state;
  EXPECT_FALSE(transport_security_state.GetDynamicDomainState(
      "www.google.com", &new_dynamic_domain_state));

  EXPECT_EQ(new_static_domain_state.sts.upgrade_mode,
            static_domain_state.sts.upgrade_mode);
  EXPECT_EQ(new_static_domain_state.sts.include_subdomains,
            static_domain_state.sts.include_subdomains);
  EXPECT_EQ(new_static_domain_state.pkp.include_subdomains,
            static_domain_state.pkp.include_subdomains);
  EXPECT_TRUE(FingerprintsEqual(new_static_domain_state.pkp.spki_hashes,
                                static_domain_state.pkp.spki_hashes));
  EXPECT_TRUE(FingerprintsEqual(new_static_domain_state.pkp.bad_spki_hashes,
                                static_domain_state.pkp.bad_spki_hashes));
}

// Make sure HSTS preserves a POST request's method and body.
TEST_F(HTTPSRequestTest, HSTSPreservesPosts) {
  static const char kData[] = "hello world";

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());


  // Per spec, TransportSecurityState expects a domain name, rather than an IP
  // address, so a MockHostResolver is needed to redirect www.somewhere.com to
  // the SpawnedTestServer.  By default, MockHostResolver maps all hosts
  // to 127.0.0.1.
  MockHostResolver host_resolver;

  // Force https for www.somewhere.com.
  TransportSecurityState transport_security_state;
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  bool include_subdomains = false;
  transport_security_state.AddHSTS("www.somewhere.com", expiry,
                                   include_subdomains);

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.

  TestURLRequestContext context(true);
  context.set_host_resolver(&host_resolver);
  context.set_transport_security_state(&transport_security_state);
  context.set_network_delegate(&network_delegate);
  context.Init();

  TestDelegate d;
  // Navigating to https://www.somewhere.com instead of https://127.0.0.1 will
  // cause a certificate error.  Ignore the error.
  d.set_allow_certificate_errors(true);

  URLRequest req(GURL(base::StringPrintf("http://www.somewhere.com:%d/echo",
                                         test_server.host_port_pair().port())),
                 DEFAULT_PRIORITY,
                 &d,
                 &context);
  req.set_method("POST");
  req.set_upload(make_scoped_ptr(CreateSimpleUploadData(kData)));

  req.Start();
  base::RunLoop().Run();

  EXPECT_EQ("https", req.url().scheme());
  EXPECT_EQ("POST", req.method());
  EXPECT_EQ(kData, d.data_received());

  LoadTimingInfo load_timing_info;
  network_delegate.GetLoadTimingInfoBeforeRedirect(&load_timing_info);
  // LoadTimingInfo of HSTS redirects is similar to that of network cache hits
  TestLoadTimingCacheHitNoNetwork(load_timing_info);
}

TEST_F(HTTPSRequestTest, SSLv3Fallback) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_ALL;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  TestURLRequestContext context(true);
  context.Init();
  d.set_allow_certificate_errors(true);
  URLRequest r(
      test_server.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context);
  r.Start();

  base::RunLoop().Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_NE(0, d.bytes_received());
  EXPECT_EQ(static_cast<int>(SSL_CONNECTION_VERSION_SSL3),
            SSLConnectionStatusToVersion(r.ssl_info().connection_status));
  EXPECT_TRUE(r.ssl_info().connection_status & SSL_CONNECTION_VERSION_FALLBACK);
}

namespace {

class SSLClientAuthTestDelegate : public TestDelegate {
 public:
  SSLClientAuthTestDelegate() : on_certificate_requested_count_(0) {
  }
  virtual void OnCertificateRequested(
      URLRequest* request,
      SSLCertRequestInfo* cert_request_info) OVERRIDE {
    on_certificate_requested_count_++;
    base::MessageLoop::current()->Quit();
  }
  int on_certificate_requested_count() {
    return on_certificate_requested_count_;
  }
 private:
  int on_certificate_requested_count_;
};

}  // namespace

// TODO(davidben): Test the rest of the code. Specifically,
// - Filtering which certificates to select.
// - Sending a certificate back.
// - Getting a certificate request in an SSL renegotiation sending the
//   HTTP request.
TEST_F(HTTPSRequestTest, ClientAuthTest) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientAuthTestDelegate d;
  {
    URLRequest r(test_server.GetURL(std::string()),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // Send no certificate.
    // TODO(davidben): Get temporary client cert import (with keys) working on
    // all platforms so we can test sending a cert as well.
    r.ContinueWithCertificate(NULL);

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
  }
}

TEST_F(HTTPSRequestTest, ResumeTest) {
  // Test that we attempt a session resume when making two connections to the
  // same host.
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.record_resume = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientSocket::ClearSessionCache();

  {
    TestDelegate d;
    URLRequest r(test_server.GetURL("ssl-session-cache"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
  }

  reinterpret_cast<HttpCache*>(default_context_.http_transaction_factory())->
    CloseAllConnections();

  {
    TestDelegate d;
    URLRequest r(test_server.GetURL("ssl-session-cache"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    // The response will look like;
    //   insert abc
    //   lookup abc
    //   insert xyz
    //
    // With a newline at the end which makes the split think that there are
    // four lines.

    EXPECT_EQ(1, d.response_started_count());
    std::vector<std::string> lines;
    base::SplitString(d.data_received(), '\n', &lines);
    ASSERT_EQ(4u, lines.size()) << d.data_received();

    std::string session_id;

    for (size_t i = 0; i < 2; i++) {
      std::vector<std::string> parts;
      base::SplitString(lines[i], '\t', &parts);
      ASSERT_EQ(2u, parts.size());
      if (i == 0) {
        EXPECT_EQ("insert", parts[0]);
        session_id = parts[1];
      } else {
        EXPECT_EQ("lookup", parts[0]);
        EXPECT_EQ(session_id, parts[1]);
      }
    }
  }
}

TEST_F(HTTPSRequestTest, SSLSessionCacheShardTest) {
  // Test that sessions aren't resumed when the value of ssl_session_cache_shard
  // differs.
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.record_resume = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientSocket::ClearSessionCache();

  {
    TestDelegate d;
    URLRequest r(test_server.GetURL("ssl-session-cache"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
  }

  // Now create a new HttpCache with a different ssl_session_cache_shard value.
  HttpNetworkSession::Params params;
  params.host_resolver = default_context_.host_resolver();
  params.cert_verifier = default_context_.cert_verifier();
  params.transport_security_state = default_context_.transport_security_state();
  params.proxy_service = default_context_.proxy_service();
  params.ssl_config_service = default_context_.ssl_config_service();
  params.http_auth_handler_factory =
      default_context_.http_auth_handler_factory();
  params.network_delegate = &default_network_delegate_;
  params.http_server_properties = default_context_.http_server_properties();
  params.ssl_session_cache_shard = "alternate";

  scoped_ptr<net::HttpCache> cache(new net::HttpCache(
      new net::HttpNetworkSession(params),
      net::HttpCache::DefaultBackend::InMemory(0)));

  default_context_.set_http_transaction_factory(cache.get());

  {
    TestDelegate d;
    URLRequest r(test_server.GetURL("ssl-session-cache"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    // The response will look like;
    //   insert abc
    //   insert xyz
    //
    // With a newline at the end which makes the split think that there are
    // three lines.

    EXPECT_EQ(1, d.response_started_count());
    std::vector<std::string> lines;
    base::SplitString(d.data_received(), '\n', &lines);
    ASSERT_EQ(3u, lines.size());

    std::string session_id;
    for (size_t i = 0; i < 2; i++) {
      std::vector<std::string> parts;
      base::SplitString(lines[i], '\t', &parts);
      ASSERT_EQ(2u, parts.size());
      EXPECT_EQ("insert", parts[0]);
      if (i == 0) {
        session_id = parts[1];
      } else {
        EXPECT_NE(session_id, parts[1]);
      }
    }
  }
}

class HTTPSSessionTest : public testing::Test {
 public:
  HTTPSSessionTest() : default_context_(true) {
    cert_verifier_.set_default_result(net::OK);

    default_context_.set_network_delegate(&default_network_delegate_);
    default_context_.set_cert_verifier(&cert_verifier_);
    default_context_.Init();
  }
  virtual ~HTTPSSessionTest() {}

 protected:
  MockCertVerifier cert_verifier_;
  TestNetworkDelegate default_network_delegate_;  // Must outlive URLRequest.
  TestURLRequestContext default_context_;
};

// Tests that session resumption is not attempted if an invalid certificate
// is presented.
TEST_F(HTTPSSessionTest, DontResumeSessionsForInvalidCertificates) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.record_resume = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientSocket::ClearSessionCache();

  // Simulate the certificate being expired and attempt a connection.
  cert_verifier_.set_default_result(net::ERR_CERT_DATE_INVALID);
  {
    TestDelegate d;
    URLRequest r(test_server.GetURL("ssl-session-cache"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
  }

  reinterpret_cast<HttpCache*>(default_context_.http_transaction_factory())->
    CloseAllConnections();

  // Now change the certificate to be acceptable (so that the response is
  // loaded), and ensure that no session id is presented to the peer.
  cert_verifier_.set_default_result(net::OK);
  {
    TestDelegate d;
    URLRequest r(test_server.GetURL("ssl-session-cache"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    // The response will look like;
    //   insert abc
    //   insert xyz
    //
    // With a newline at the end which makes the split think that there are
    // three lines.
    //
    // If a session was presented (eg: a bug), then the response would look
    // like;
    //   insert abc
    //   lookup abc
    //   insert xyz

    EXPECT_EQ(1, d.response_started_count());
    std::vector<std::string> lines;
    base::SplitString(d.data_received(), '\n', &lines);
    ASSERT_EQ(3u, lines.size()) << d.data_received();

    std::string session_id;
    for (size_t i = 0; i < 2; i++) {
      std::vector<std::string> parts;
      base::SplitString(lines[i], '\t', &parts);
      ASSERT_EQ(2u, parts.size());
      EXPECT_EQ("insert", parts[0]);
      if (i == 0) {
        session_id = parts[1];
      } else {
        EXPECT_NE(session_id, parts[1]);
      }
    }
  }
}

class TestSSLConfigService : public SSLConfigService {
 public:
  TestSSLConfigService(bool ev_enabled,
                       bool online_rev_checking,
                       bool rev_checking_required_local_anchors)
      : ev_enabled_(ev_enabled),
        online_rev_checking_(online_rev_checking),
        rev_checking_required_local_anchors_(
            rev_checking_required_local_anchors) {}

  // SSLConfigService:
  virtual void GetSSLConfig(SSLConfig* config) OVERRIDE {
    *config = SSLConfig();
    config->rev_checking_enabled = online_rev_checking_;
    config->verify_ev_cert = ev_enabled_;
    config->rev_checking_required_local_anchors =
        rev_checking_required_local_anchors_;
  }

 protected:
  virtual ~TestSSLConfigService() {}

 private:
  const bool ev_enabled_;
  const bool online_rev_checking_;
  const bool rev_checking_required_local_anchors_;
};

// This the fingerprint of the "Testing CA" certificate used by the testserver.
// See net/data/ssl/certificates/ocsp-test-root.pem.
static const SHA1HashValue kOCSPTestCertFingerprint =
  { { 0xf1, 0xad, 0xf6, 0xce, 0x42, 0xac, 0xe7, 0xb4, 0xf4, 0x24,
      0xdb, 0x1a, 0xf7, 0xa0, 0x9f, 0x09, 0xa1, 0xea, 0xf1, 0x5c } };

// This is the SHA256, SPKI hash of the "Testing CA" certificate used by the
// testserver.
static const SHA256HashValue kOCSPTestCertSPKI = { {
  0xee, 0xe6, 0x51, 0x2d, 0x4c, 0xfa, 0xf7, 0x3e,
  0x6c, 0xd8, 0xca, 0x67, 0xed, 0xb5, 0x5d, 0x49,
  0x76, 0xe1, 0x52, 0xa7, 0x6e, 0x0e, 0xa0, 0x74,
  0x09, 0x75, 0xe6, 0x23, 0x24, 0xbd, 0x1b, 0x28,
} };

// This is the policy OID contained in the certificates that testserver
// generates.
static const char kOCSPTestCertPolicy[] = "1.3.6.1.4.1.11129.2.4.1";

class HTTPSOCSPTest : public HTTPSRequestTest {
 public:
  HTTPSOCSPTest()
      : context_(true),
        ev_test_policy_(
            new ScopedTestEVPolicy(EVRootCAMetadata::GetInstance(),
                                   kOCSPTestCertFingerprint,
                                   kOCSPTestCertPolicy)) {
  }

  virtual void SetUp() OVERRIDE {
    SetupContext(&context_);
    context_.Init();

    scoped_refptr<net::X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ocsp-test-root.pem");
    CHECK_NE(static_cast<X509Certificate*>(NULL), root_cert);
    test_root_.reset(new ScopedTestRoot(root_cert.get()));

#if defined(USE_NSS) || defined(OS_IOS)
    SetURLRequestContextForNSSHttpIO(&context_);
    EnsureNSSHttpIOInit();
#endif
  }

  void DoConnection(const SpawnedTestServer::SSLOptions& ssl_options,
                    CertStatus* out_cert_status) {
    // We always overwrite out_cert_status.
    *out_cert_status = 0;
    SpawnedTestServer test_server(
        SpawnedTestServer::TYPE_HTTPS,
        ssl_options,
        base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    ASSERT_TRUE(test_server.Start());

    TestDelegate d;
    d.set_allow_certificate_errors(true);
    URLRequest r(
        test_server.GetURL(std::string()), DEFAULT_PRIORITY, &d, &context_);
    r.Start();

    base::RunLoop().Run();

    EXPECT_EQ(1, d.response_started_count());
    *out_cert_status = r.ssl_info().cert_status;
  }

  virtual ~HTTPSOCSPTest() {
#if defined(USE_NSS) || defined(OS_IOS)
    ShutdownNSSHttpIO();
#endif
  }

 protected:
  // SetupContext configures the URLRequestContext that will be used for making
  // connetions to testserver. This can be overridden in test subclasses for
  // different behaviour.
  virtual void SetupContext(URLRequestContext* context) {
    context->set_ssl_config_service(
        new TestSSLConfigService(true /* check for EV */,
                                 true /* online revocation checking */,
                                 false /* require rev. checking for local
                                          anchors */));
  }

  scoped_ptr<ScopedTestRoot> test_root_;
  TestURLRequestContext context_;
  scoped_ptr<ScopedTestEVPolicy> ev_test_policy_;
};

static CertStatus ExpectedCertStatusForFailedOnlineRevocationCheck() {
#if defined(OS_WIN)
  // Windows can return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION but we don't
  // have that ability on other platforms.
  return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
#else
  return 0;
#endif
}

// SystemSupportsHardFailRevocationChecking returns true iff the current
// operating system supports revocation checking and can distinguish between
// situations where a given certificate lacks any revocation information (eg:
// no CRLDistributionPoints and no OCSP Responder AuthorityInfoAccess) and when
// revocation information cannot be obtained (eg: the CRL was unreachable).
// If it does not, then tests which rely on 'hard fail' behaviour should be
// skipped.
static bool SystemSupportsHardFailRevocationChecking() {
#if defined(OS_WIN) || defined(USE_NSS) || defined(OS_IOS)
  return true;
#else
  return false;
#endif
}

// SystemUsesChromiumEVMetadata returns true iff the current operating system
// uses Chromium's EV metadata (i.e. EVRootCAMetadata). If it does not, then
// several tests are effected because our testing EV certificate won't be
// recognised as EV.
static bool SystemUsesChromiumEVMetadata() {
#if defined(USE_OPENSSL_CERTS) && !defined(OS_ANDROID)
  // http://crbug.com/117478 - OpenSSL does not support EV validation.
  return false;
#elif (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_ANDROID)
  // On OS X and Android, we use the system to tell us whether a certificate is
  // EV or not and the system won't recognise our testing root.
  return false;
#else
  return true;
#endif
}

static bool SystemSupportsOCSP() {
#if defined(USE_OPENSSL)
  // http://crbug.com/117478 - OpenSSL does not support OCSP.
  return false;
#elif defined(OS_WIN)
  return base::win::GetVersion() >= base::win::VERSION_VISTA;
#elif defined(OS_ANDROID)
  // TODO(jnd): http://crbug.com/117478 - EV verification is not yet supported.
  return false;
#else
  return true;
#endif
}

TEST_F(HTTPSOCSPTest, Valid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Revoked) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

#if !(defined(OS_MACOSX) && !defined(OS_IOS))
  // Doesn't pass on OS X yet for reasons that need to be investigated.
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
#endif
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Invalid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(ExpectedCertStatusForFailedOnlineRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  // Without a positive OCSP response, we shouldn't show the EV status.
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

class HTTPSHardFailTest : public HTTPSOCSPTest {
 protected:
  virtual void SetupContext(URLRequestContext* context) OVERRIDE {
    context->set_ssl_config_service(
        new TestSSLConfigService(false /* check for EV */,
                                 false /* online revocation checking */,
                                 true /* require rev. checking for local
                                         anchors */));
  }
};


TEST_F(HTTPSHardFailTest, FailsOnOCSPInvalid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED,
            cert_status & CERT_STATUS_REVOKED);

  // Without a positive OCSP response, we shouldn't show the EV status.
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

class HTTPSEVCRLSetTest : public HTTPSOCSPTest {
 protected:
  virtual void SetupContext(URLRequestContext* context) OVERRIDE {
    context->set_ssl_config_service(
        new TestSSLConfigService(true /* check for EV */,
                                 false /* online revocation checking */,
                                 false /* require rev. checking for local
                                          anchors */));
  }
};

TEST_F(HTTPSEVCRLSetTest, MissingCRLSetAndInvalidOCSP) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;
  SSLConfigService::SetCRLSet(scoped_refptr<CRLSet>());

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(ExpectedCertStatusForFailedOnlineRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, MissingCRLSetAndRevokedOCSP) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;
  SSLConfigService::SetCRLSet(scoped_refptr<CRLSet>());

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  // Currently only works for Windows. When using NSS or OS X, it's not
  // possible to determine whether the check failed because of actual
  // revocation or because there was an OCSP failure.
#if defined(OS_WIN)
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
#else
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
#endif

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, MissingCRLSetAndGoodOCSP) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  SSLConfigService::SetCRLSet(scoped_refptr<CRLSet>());

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, ExpiredCRLSet) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;
  SSLConfigService::SetCRLSet(
      scoped_refptr<CRLSet>(CRLSet::ExpiredCRLSetForTesting()));

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(ExpectedCertStatusForFailedOnlineRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, FreshCRLSetCovered) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;
  SSLConfigService::SetCRLSet(
      scoped_refptr<CRLSet>(CRLSet::ForTesting(
          false, &kOCSPTestCertSPKI, "")));

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  // With a fresh CRLSet that covers the issuing certificate, we shouldn't do a
  // revocation check for EV.
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));
  EXPECT_FALSE(
      static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, FreshCRLSetNotCovered) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;
  SSLConfigService::SetCRLSet(
      scoped_refptr<CRLSet>(CRLSet::EmptyCRLSetForTesting()));

  CertStatus cert_status = 0;
  DoConnection(ssl_options, &cert_status);

  // Even with a fresh CRLSet, we should still do online revocation checks when
  // the certificate chain isn't covered by the CRLSet, which it isn't in this
  // test.
  EXPECT_EQ(ExpectedCertStatusForFailedOnlineRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, ExpiredCRLSetAndRevokedNonEVCert) {
  // Test that when EV verification is requested, but online revocation
  // checking is disabled, and the leaf certificate is not in fact EV, that
  // no revocation checking actually happens.
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  // Unmark the certificate's OID as EV, which should disable revocation
  // checking (as per the user preference)
  ev_test_policy_.reset();

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;
  SSLConfigService::SetCRLSet(
      scoped_refptr<CRLSet>(CRLSet::ExpiredCRLSetForTesting()));

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

class HTTPSCRLSetTest : public HTTPSOCSPTest {
 protected:
  virtual void SetupContext(URLRequestContext* context) OVERRIDE {
    context->set_ssl_config_service(
        new TestSSLConfigService(false /* check for EV */,
                                 false /* online revocation checking */,
                                 false /* require rev. checking for local
                                          anchors */));
  }
};

TEST_F(HTTPSCRLSetTest, ExpiredCRLSet) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_INVALID;
  SSLConfigService::SetCRLSet(
      scoped_refptr<CRLSet>(CRLSet::ExpiredCRLSetForTesting()));

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  // If we're not trying EV verification then, even if the CRLSet has expired,
  // we don't fall back to online revocation checks.
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSCRLSetTest, CRLSetRevoked) {
#if defined(USE_OPENSSL)
  LOG(WARNING) << "Skipping test because system doesn't support CRLSets";
  return;
#endif

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.cert_serial = 10;
  SSLConfigService::SetCRLSet(
      scoped_refptr<CRLSet>(CRLSet::ForTesting(
          false, &kOCSPTestCertSPKI, "\x0a")));

  CertStatus cert_status = 0;
  DoConnection(ssl_options, &cert_status);

  // If the certificate is recorded as revoked in the CRLSet, that should be
  // reflected without online revocation checking.
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(
      static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}
#endif  // !defined(OS_IOS)

#if !defined(DISABLE_FTP_SUPPORT)
class URLRequestTestFTP : public URLRequestTest {
 public:
  URLRequestTestFTP()
      : test_server_(SpawnedTestServer::TYPE_FTP, SpawnedTestServer::kLocalhost,
                     base::FilePath()) {
  }

 protected:
  SpawnedTestServer test_server_;
};

// Make sure an FTP request using an unsafe ports fails.
TEST_F(URLRequestTestFTP, UnsafePort) {
  ASSERT_TRUE(test_server_.Start());

  URLRequestJobFactoryImpl job_factory;
  FtpNetworkLayer ftp_transaction_factory(default_context_.host_resolver());

  GURL url("ftp://127.0.0.1:7");
  job_factory.SetProtocolHandler(
      "ftp",
      new FtpProtocolHandler(&ftp_transaction_factory));
  default_context_.set_job_factory(&job_factory);

  TestDelegate d;
  {
    URLRequest r(url, DEFAULT_PRIORITY, &d, &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_UNSAFE_PORT, r.status().error());
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPDirectoryListing) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURL("/"), DEFAULT_PRIORITY, &d, &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_LT(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPGetTestAnonymous) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    URLRequest r(test_server_.GetURL("/LICENSE"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPGetTest) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    URLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE", "chrome", "chrome"),
        DEFAULT_PRIORITY,
        &d,
        &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));

    LoadTimingInfo load_timing_info;
    r.GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNoHttpResponse(load_timing_info);
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPCheckWrongPassword) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    URLRequest r(test_server_.GetURLWithUserAndPassword(
                     "/LICENSE", "chrome", "wrong_password"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPCheckWrongPasswordRestart) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d.set_credentials(AuthCredentials(kChrome, kChrome));
  {
    URLRequest r(test_server_.GetURLWithUserAndPassword(
                     "/LICENSE", "chrome", "wrong_password"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPCheckWrongUser) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    URLRequest r(test_server_.GetURLWithUserAndPassword(
                     "/LICENSE", "wrong_user", "chrome"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPCheckWrongUserRestart) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d.set_credentials(AuthCredentials(kChrome, kChrome));
  {
    URLRequest r(test_server_.GetURLWithUserAndPassword(
                     "/LICENSE", "wrong_user", "chrome"),
                 DEFAULT_PRIORITY,
                 &d,
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPCacheURLCredentials) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");

  scoped_ptr<TestDelegate> d(new TestDelegate);
  {
    // Pass correct login identity in the URL.
    URLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE", "chrome", "chrome"),
        DEFAULT_PRIORITY,
        d.get(),
        &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }

  d.reset(new TestDelegate);
  {
    // This request should use cached identity from previous request.
    URLRequest r(test_server_.GetURL("/LICENSE"),
                 DEFAULT_PRIORITY,
                 d.get(),
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, DISABLED_FTPCacheLoginBoxCredentials) {
  ASSERT_TRUE(test_server_.Start());

  base::FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");

  scoped_ptr<TestDelegate> d(new TestDelegate);
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d->set_credentials(AuthCredentials(kChrome, kChrome));
  {
    URLRequest r(test_server_.GetURLWithUserAndPassword(
                     "/LICENSE", "chrome", "wrong_password"),
                 DEFAULT_PRIORITY,
                 d.get(),
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }

  // Use a new delegate without explicit credentials. The cached ones should be
  // used.
  d.reset(new TestDelegate);
  {
    // Don't pass wrong credentials in the URL, they would override valid cached
    // ones.
    URLRequest r(test_server_.GetURL("/LICENSE"),
                 DEFAULT_PRIORITY,
                 d.get(),
                 &default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    base::RunLoop().Run();

    int64 file_size = 0;
    base::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }
}
#endif  // !defined(DISABLE_FTP_SUPPORT)

}  // namespace net
