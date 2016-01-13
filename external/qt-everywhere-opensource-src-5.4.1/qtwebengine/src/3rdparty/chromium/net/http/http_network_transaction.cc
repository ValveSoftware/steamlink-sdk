// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_status_code.h"
#include "net/http/http_stream_base.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/url_security_manager.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/hpack_huffman_aggregator.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "url/gurl.h"

#if defined(SPDY_PROXY_AUTH_ORIGIN)
#include <algorithm>
#include "net/proxy/proxy_server.h"
#endif


using base::Time;
using base::TimeDelta;

namespace net {

namespace {

void ProcessAlternateProtocol(
    HttpNetworkSession* session,
    const HttpResponseHeaders& headers,
    const HostPortPair& http_host_port_pair) {
  std::string alternate_protocol_str;

  if (!headers.EnumerateHeader(NULL, kAlternateProtocolHeader,
                               &alternate_protocol_str)) {
    // Header is not present.
    return;
  }

  session->http_stream_factory()->ProcessAlternateProtocol(
      session->http_server_properties(),
      alternate_protocol_str,
      http_host_port_pair,
      *session);
}

// Returns true if |error| is a client certificate authentication error.
bool IsClientCertificateError(int error) {
  switch (error) {
    case ERR_BAD_SSL_CLIENT_AUTH_CERT:
    case ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED:
    case ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY:
    case ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED:
      return true;
    default:
      return false;
  }
}

base::Value* NetLogSSLVersionFallbackCallback(
    const GURL* url,
    int net_error,
    uint16 version_before,
    uint16 version_after,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("host_and_port", GetHostAndPort(*url));
  dict->SetInteger("net_error", net_error);
  dict->SetInteger("version_before", version_before);
  dict->SetInteger("version_after", version_after);
  return dict;
}

}  // namespace

//-----------------------------------------------------------------------------

HttpNetworkTransaction::HttpNetworkTransaction(RequestPriority priority,
                                               HttpNetworkSession* session)
    : pending_auth_target_(HttpAuth::AUTH_NONE),
      io_callback_(base::Bind(&HttpNetworkTransaction::OnIOComplete,
                              base::Unretained(this))),
      session_(session),
      request_(NULL),
      priority_(priority),
      headers_valid_(false),
      logged_response_time_(false),
      fallback_error_code_(ERR_SSL_INAPPROPRIATE_FALLBACK),
      request_headers_(),
      read_buf_len_(0),
      total_received_bytes_(0),
      next_state_(STATE_NONE),
      establishing_tunnel_(false),
      websocket_handshake_stream_base_create_helper_(NULL) {
  session->ssl_config_service()->GetSSLConfig(&server_ssl_config_);
  session->GetNextProtos(&server_ssl_config_.next_protos);
  proxy_ssl_config_ = server_ssl_config_;
}

HttpNetworkTransaction::~HttpNetworkTransaction() {
  if (stream_.get()) {
    HttpResponseHeaders* headers = GetResponseHeaders();
    // TODO(mbelshe): The stream_ should be able to compute whether or not the
    //                stream should be kept alive.  No reason to compute here
    //                and pass it in.
    bool try_to_keep_alive =
        next_state_ == STATE_NONE &&
        stream_->CanFindEndOfResponse() &&
        (!headers || headers->IsKeepAlive());
    if (!try_to_keep_alive) {
      stream_->Close(true /* not reusable */);
    } else {
      if (stream_->IsResponseBodyComplete()) {
        // If the response body is complete, we can just reuse the socket.
        stream_->Close(false /* reusable */);
      } else if (stream_->IsSpdyHttpStream()) {
        // Doesn't really matter for SpdyHttpStream. Just close it.
        stream_->Close(true /* not reusable */);
      } else {
        // Otherwise, we try to drain the response body.
        HttpStreamBase* stream = stream_.release();
        stream->Drain(session_);
      }
    }
  }

  if (request_ && request_->upload_data_stream)
    request_->upload_data_stream->Reset();  // Invalidate pending callbacks.
}

int HttpNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  const CompletionCallback& callback,
                                  const BoundNetLog& net_log) {
  SIMPLE_STATS_COUNTER("HttpNetworkTransaction.Count");

  net_log_ = net_log;
  request_ = request_info;
  start_time_ = base::Time::Now();

  if (request_->load_flags & LOAD_DISABLE_CERT_REVOCATION_CHECKING) {
    server_ssl_config_.rev_checking_enabled = false;
    proxy_ssl_config_.rev_checking_enabled = false;
  }

  // Channel ID is disabled if privacy mode is enabled for this request.
  if (request_->privacy_mode == PRIVACY_MODE_ENABLED)
    server_ssl_config_.channel_id_enabled = false;

  next_state_ = STATE_NOTIFY_BEFORE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartIgnoringLastError(
    const CompletionCallback& callback) {
  DCHECK(!stream_.get());
  DCHECK(!stream_request_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_CREATE_STREAM;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert, const CompletionCallback& callback) {
  // In HandleCertificateRequest(), we always tear down existing stream
  // requests to force a new connection.  So we shouldn't have one here.
  DCHECK(!stream_request_.get());
  DCHECK(!stream_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  SSLConfig* ssl_config = response_.cert_request_info->is_proxy ?
      &proxy_ssl_config_ : &server_ssl_config_;
  ssl_config->send_client_cert = true;
  ssl_config->client_cert = client_cert;
  session_->ssl_client_auth_cache()->Add(
      response_.cert_request_info->host_and_port, client_cert);
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  next_state_ = STATE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithAuth(
    const AuthCredentials& credentials, const CompletionCallback& callback) {
  HttpAuth::Target target = pending_auth_target_;
  if (target == HttpAuth::AUTH_NONE) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  pending_auth_target_ = HttpAuth::AUTH_NONE;

  auth_controllers_[target]->ResetAuth(credentials);

  DCHECK(callback_.is_null());

  int rv = OK;
  if (target == HttpAuth::AUTH_PROXY && establishing_tunnel_) {
    // In this case, we've gathered credentials for use with proxy
    // authentication of a tunnel.
    DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
    DCHECK(stream_request_ != NULL);
    auth_controllers_[target] = NULL;
    ResetStateForRestart();
    rv = stream_request_->RestartTunnelWithProxyAuth(credentials);
  } else {
    // In this case, we've gathered credentials for the server or the proxy
    // but it is not during the tunneling phase.
    DCHECK(stream_request_ == NULL);
    PrepareForAuthRestart(target);
    rv = DoLoop(OK);
  }

  if (rv == ERR_IO_PENDING)
    callback_ = callback;
  return rv;
}

void HttpNetworkTransaction::PrepareForAuthRestart(HttpAuth::Target target) {
  DCHECK(HaveAuth(target));
  DCHECK(!stream_request_.get());

  bool keep_alive = false;
  // Even if the server says the connection is keep-alive, we have to be
  // able to find the end of each response in order to reuse the connection.
  if (GetResponseHeaders()->IsKeepAlive() &&
      stream_->CanFindEndOfResponse()) {
    // If the response body hasn't been completely read, we need to drain
    // it first.
    if (!stream_->IsResponseBodyComplete()) {
      next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
      read_buf_ = new IOBuffer(kDrainBodyBufferSize);  // A bit bucket.
      read_buf_len_ = kDrainBodyBufferSize;
      return;
    }
    keep_alive = true;
  }

  // We don't need to drain the response body, so we act as if we had drained
  // the response body.
  DidDrainBodyForAuthRestart(keep_alive);
}

void HttpNetworkTransaction::DidDrainBodyForAuthRestart(bool keep_alive) {
  DCHECK(!stream_request_.get());

  if (stream_.get()) {
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    HttpStream* new_stream = NULL;
    if (keep_alive && stream_->IsConnectionReusable()) {
      // We should call connection_->set_idle_time(), but this doesn't occur
      // often enough to be worth the trouble.
      stream_->SetConnectionReused();
      new_stream =
          static_cast<HttpStream*>(stream_.get())->RenewStreamForAuth();
    }

    if (!new_stream) {
      // Close the stream and mark it as not_reusable.  Even in the
      // keep_alive case, we've determined that the stream_ is not
      // reusable if new_stream is NULL.
      stream_->Close(true);
      next_state_ = STATE_CREATE_STREAM;
    } else {
      // Renewed streams shouldn't carry over received bytes.
      DCHECK_EQ(0, new_stream->GetTotalReceivedBytes());
      next_state_ = STATE_INIT_STREAM;
    }
    stream_.reset(new_stream);
  }

  // Reset the other member variables.
  ResetStateForAuthRestart();
}

bool HttpNetworkTransaction::IsReadyToRestartForAuth() {
  return pending_auth_target_ != HttpAuth::AUTH_NONE &&
      HaveAuth(pending_auth_target_);
}

int HttpNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_LT(0, buf_len);

  State next_state = STATE_NONE;

  scoped_refptr<HttpResponseHeaders> headers(GetResponseHeaders());
  if (headers_valid_ && headers.get() && stream_request_.get()) {
    // We're trying to read the body of the response but we're still trying
    // to establish an SSL tunnel through an HTTP proxy.  We can't read these
    // bytes when establishing a tunnel because they might be controlled by
    // an active network attacker.  We don't worry about this for HTTP
    // because an active network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.  We
    // also don't worry about this for an HTTPS Proxy, because the
    // communication with the proxy is secure.
    // See http://crbug.com/8473.
    DCHECK(proxy_info_.is_http() || proxy_info_.is_https());
    DCHECK_EQ(headers->response_code(), HTTP_PROXY_AUTHENTICATION_REQUIRED);
    LOG(WARNING) << "Blocked proxy response with status "
                 << headers->response_code() << " to CONNECT request for "
                 << GetHostAndPort(request_->url) << ".";
    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  // Are we using SPDY or HTTP?
  next_state = STATE_READ_BODY;

  read_buf_ = buf;
  read_buf_len_ = buf_len;

  next_state_ = next_state;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;
  return rv;
}

void HttpNetworkTransaction::StopCaching() {}

bool HttpNetworkTransaction::GetFullRequestHeaders(
    HttpRequestHeaders* headers) const {
  // TODO(ttuttle): Make sure we've populated request_headers_.
  *headers = request_headers_;
  return true;
}

int64 HttpNetworkTransaction::GetTotalReceivedBytes() const {
  int64 total_received_bytes = total_received_bytes_;
  if (stream_)
    total_received_bytes += stream_->GetTotalReceivedBytes();
  return total_received_bytes;
}

void HttpNetworkTransaction::DoneReading() {}

const HttpResponseInfo* HttpNetworkTransaction::GetResponseInfo() const {
  return ((headers_valid_ && response_.headers.get()) ||
          response_.ssl_info.cert.get() || response_.cert_request_info.get())
             ? &response_
             : NULL;
}

LoadState HttpNetworkTransaction::GetLoadState() const {
  // TODO(wtc): Define a new LoadState value for the
  // STATE_INIT_CONNECTION_COMPLETE state, which delays the HTTP request.
  switch (next_state_) {
    case STATE_CREATE_STREAM:
      return LOAD_STATE_WAITING_FOR_DELEGATE;
    case STATE_CREATE_STREAM_COMPLETE:
      return stream_request_->GetLoadState();
    case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
    case STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE:
    case STATE_SEND_REQUEST_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_READ_HEADERS_COMPLETE:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_READ_BODY_COMPLETE:
      return LOAD_STATE_READING_RESPONSE;
    default:
      return LOAD_STATE_IDLE;
  }
}

UploadProgress HttpNetworkTransaction::GetUploadProgress() const {
  if (!stream_.get())
    return UploadProgress();

  // TODO(bashi): This cast is temporary. Remove later.
  return static_cast<HttpStream*>(stream_.get())->GetUploadProgress();
}

void HttpNetworkTransaction::SetQuicServerInfo(
    QuicServerInfo* quic_server_info) {}

bool HttpNetworkTransaction::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  if (!stream_ || !stream_->GetLoadTimingInfo(load_timing_info))
    return false;

  load_timing_info->proxy_resolve_start =
      proxy_info_.proxy_resolve_start_time();
  load_timing_info->proxy_resolve_end = proxy_info_.proxy_resolve_end_time();
  load_timing_info->send_start = send_start_time_;
  load_timing_info->send_end = send_end_time_;
  return true;
}

void HttpNetworkTransaction::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (stream_request_)
    stream_request_->SetPriority(priority);
  if (stream_)
    stream_->SetPriority(priority);
}

void HttpNetworkTransaction::SetWebSocketHandshakeStreamCreateHelper(
    WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  websocket_handshake_stream_base_create_helper_ = create_helper;
}

void HttpNetworkTransaction::SetBeforeNetworkStartCallback(
    const BeforeNetworkStartCallback& callback) {
  before_network_start_callback_ = callback;
}

int HttpNetworkTransaction::ResumeNetworkStart() {
  DCHECK_EQ(next_state_, STATE_CREATE_STREAM);
  return DoLoop(OK);
}

void HttpNetworkTransaction::OnStreamReady(const SSLConfig& used_ssl_config,
                                           const ProxyInfo& used_proxy_info,
                                           HttpStreamBase* stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK(stream_request_.get());

  if (stream_)
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
  stream_.reset(stream);
  server_ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;
  response_.was_npn_negotiated = stream_request_->was_npn_negotiated();
  response_.npn_negotiated_protocol = SSLClientSocket::NextProtoToString(
      stream_request_->protocol_negotiated());
  response_.was_fetched_via_spdy = stream_request_->using_spdy();
  response_.was_fetched_via_proxy = !proxy_info_.is_direct();
  if (response_.was_fetched_via_proxy && !proxy_info_.is_empty())
    response_.proxy_server = proxy_info_.proxy_server().host_port_pair();
  OnIOComplete(OK);
}

void HttpNetworkTransaction::OnWebSocketHandshakeStreamReady(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    WebSocketHandshakeStreamBase* stream) {
  OnStreamReady(used_ssl_config, used_proxy_info, stream);
}

void HttpNetworkTransaction::OnStreamFailed(int result,
                                            const SSLConfig& used_ssl_config) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());
  server_ssl_config_ = used_ssl_config;

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnCertificateError(
    int result,
    const SSLConfig& used_ssl_config,
    const SSLInfo& ssl_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());

  response_.ssl_info = ssl_info;
  server_ssl_config_ = used_ssl_config;

  // TODO(mbelshe):  For now, we're going to pass the error through, and that
  // will close the stream_request in all cases.  This means that we're always
  // going to restart an entire STATE_CREATE_STREAM, even if the connection is
  // good and the user chooses to ignore the error.  This is not ideal, but not
  // the end of the world either.

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  DCHECK(stream_request_.get());
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  establishing_tunnel_ = true;
  response_.headers = proxy_response.headers;
  response_.auth_challenge = proxy_response.auth_challenge;
  headers_valid_ = true;
  server_ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;

  auth_controllers_[HttpAuth::AUTH_PROXY] = auth_controller;
  pending_auth_target_ = HttpAuth::AUTH_PROXY;

  DoCallback(OK);
}

void HttpNetworkTransaction::OnNeedsClientAuth(
    const SSLConfig& used_ssl_config,
    SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  server_ssl_config_ = used_ssl_config;
  response_.cert_request_info = cert_info;
  OnIOComplete(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void HttpNetworkTransaction::OnHttpsProxyTunnelResponse(
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStreamBase* stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  headers_valid_ = true;
  response_ = response_info;
  server_ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;
  if (stream_)
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
  stream_.reset(stream);
  stream_request_.reset();  // we're done with the stream request
  OnIOComplete(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);
}

bool HttpNetworkTransaction::is_https_request() const {
  return request_->url.SchemeIs("https");
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!callback_.is_null());

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback c = callback_;
  callback_.Reset();
  c.Run(rv);
}

void HttpNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int HttpNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_NOTIFY_BEFORE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoNotifyBeforeCreateStream();
        break;
      case STATE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case STATE_CREATE_STREAM_COMPLETE:
        rv = DoCreateStreamComplete(rv);
        break;
      case STATE_INIT_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoInitStream();
        break;
      case STATE_INIT_STREAM_COMPLETE:
        rv = DoInitStreamComplete(rv);
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateProxyAuthToken();
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateProxyAuthTokenComplete(rv);
        break;
      case STATE_GENERATE_SERVER_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateServerAuthToken();
        break;
      case STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateServerAuthTokenComplete(rv);
        break;
      case STATE_INIT_REQUEST_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoInitRequestBody();
        break;
      case STATE_INIT_REQUEST_BODY_COMPLETE:
        rv = DoInitRequestBodyComplete(rv);
        break;
      case STATE_BUILD_REQUEST:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST);
        rv = DoBuildRequest();
        break;
      case STATE_BUILD_REQUEST_COMPLETE:
        rv = DoBuildRequestComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST, rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS, rv);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_BODY);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_READ_BODY, rv);
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART);
        rv = DoDrainBodyForAuthRestart();
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE:
        rv = DoDrainBodyForAuthRestartComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART, rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpNetworkTransaction::DoNotifyBeforeCreateStream() {
  next_state_ = STATE_CREATE_STREAM;
  bool defer = false;
  if (!before_network_start_callback_.is_null())
    before_network_start_callback_.Run(&defer);
  if (!defer)
    return OK;
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStream() {
  next_state_ = STATE_CREATE_STREAM_COMPLETE;
  if (ForWebSocketHandshake()) {
    stream_request_.reset(
        session_->http_stream_factory_for_websocket()
            ->RequestWebSocketHandshakeStream(
                  *request_,
                  priority_,
                  server_ssl_config_,
                  proxy_ssl_config_,
                  this,
                  websocket_handshake_stream_base_create_helper_,
                  net_log_));
  } else {
    stream_request_.reset(
        session_->http_stream_factory()->RequestStream(
            *request_,
            priority_,
            server_ssl_config_,
            proxy_ssl_config_,
            this,
            net_log_));
  }
  DCHECK(stream_request_.get());
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStreamComplete(int result) {
  if (result == OK) {
    next_state_ = STATE_INIT_STREAM;
    DCHECK(stream_.get());
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    result = HandleCertificateRequest(result);
  } else if (result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    // Return OK and let the caller read the proxy's error page
    next_state_ = STATE_NONE;
    return OK;
  }

  // Handle possible handshake errors that may have occurred if the stream
  // used SSL for one or more of the layers.
  result = HandleSSLHandshakeError(result);

  // At this point we are done with the stream_request_.
  stream_request_.reset();
  return result;
}

int HttpNetworkTransaction::DoInitStream() {
  DCHECK(stream_.get());
  next_state_ = STATE_INIT_STREAM_COMPLETE;
  return stream_->InitializeStream(request_, priority_, net_log_, io_callback_);
}

int HttpNetworkTransaction::DoInitStreamComplete(int result) {
  if (result == OK) {
    next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  } else {
    if (result < 0)
      result = HandleIOError(result);

    // The stream initialization failed, so this stream will never be useful.
    if (stream_)
        total_received_bytes_ += stream_->GetTotalReceivedBytes();
    stream_.reset();
  }

  return result;
}

int HttpNetworkTransaction::DoGenerateProxyAuthToken() {
  next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE;
  if (!ShouldApplyProxyAuth())
    return OK;
  HttpAuth::Target target = HttpAuth::AUTH_PROXY;
  if (!auth_controllers_[target].get())
    auth_controllers_[target] =
        new HttpAuthController(target,
                               AuthURL(target),
                               session_->http_auth_cache(),
                               session_->http_auth_handler_factory());
  return auth_controllers_[target]->MaybeGenerateAuthToken(request_,
                                                           io_callback_,
                                                           net_log_);
}

int HttpNetworkTransaction::DoGenerateProxyAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv == OK)
    next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN;
  return rv;
}

int HttpNetworkTransaction::DoGenerateServerAuthToken() {
  next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE;
  HttpAuth::Target target = HttpAuth::AUTH_SERVER;
  if (!auth_controllers_[target].get()) {
    auth_controllers_[target] =
        new HttpAuthController(target,
                               AuthURL(target),
                               session_->http_auth_cache(),
                               session_->http_auth_handler_factory());
    if (request_->load_flags & LOAD_DO_NOT_USE_EMBEDDED_IDENTITY)
      auth_controllers_[target]->DisableEmbeddedIdentity();
  }
  if (!ShouldApplyServerAuth())
    return OK;
  return auth_controllers_[target]->MaybeGenerateAuthToken(request_,
                                                           io_callback_,
                                                           net_log_);
}

int HttpNetworkTransaction::DoGenerateServerAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv == OK)
    next_state_ = STATE_INIT_REQUEST_BODY;
  return rv;
}

void HttpNetworkTransaction::BuildRequestHeaders(bool using_proxy) {
  request_headers_.SetHeader(HttpRequestHeaders::kHost,
                             GetHostAndOptionalPort(request_->url));

  // For compat with HTTP/1.0 servers and proxies:
  if (using_proxy) {
    request_headers_.SetHeader(HttpRequestHeaders::kProxyConnection,
                               "keep-alive");
  } else {
    request_headers_.SetHeader(HttpRequestHeaders::kConnection, "keep-alive");
  }

  // Add a content length header?
  if (request_->upload_data_stream) {
    if (request_->upload_data_stream->is_chunked()) {
      request_headers_.SetHeader(
          HttpRequestHeaders::kTransferEncoding, "chunked");
    } else {
      request_headers_.SetHeader(
          HttpRequestHeaders::kContentLength,
          base::Uint64ToString(request_->upload_data_stream->size()));
    }
  } else if (request_->method == "POST" || request_->method == "PUT" ||
             request_->method == "HEAD") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    request_headers_.SetHeader(HttpRequestHeaders::kContentLength, "0");
  }

  // Honor load flags that impact proxy caches.
  if (request_->load_flags & LOAD_BYPASS_CACHE) {
    request_headers_.SetHeader(HttpRequestHeaders::kPragma, "no-cache");
    request_headers_.SetHeader(HttpRequestHeaders::kCacheControl, "no-cache");
  } else if (request_->load_flags & LOAD_VALIDATE_CACHE) {
    request_headers_.SetHeader(HttpRequestHeaders::kCacheControl, "max-age=0");
  }

  if (ShouldApplyProxyAuth() && HaveAuth(HttpAuth::AUTH_PROXY))
    auth_controllers_[HttpAuth::AUTH_PROXY]->AddAuthorizationHeader(
        &request_headers_);
  if (ShouldApplyServerAuth() && HaveAuth(HttpAuth::AUTH_SERVER))
    auth_controllers_[HttpAuth::AUTH_SERVER]->AddAuthorizationHeader(
        &request_headers_);

  request_headers_.MergeFrom(request_->extra_headers);
  response_.did_use_http_auth =
      request_headers_.HasHeader(HttpRequestHeaders::kAuthorization) ||
      request_headers_.HasHeader(HttpRequestHeaders::kProxyAuthorization);
}

int HttpNetworkTransaction::DoInitRequestBody() {
  next_state_ = STATE_INIT_REQUEST_BODY_COMPLETE;
  int rv = OK;
  if (request_->upload_data_stream)
    rv = request_->upload_data_stream->Init(io_callback_);
  return rv;
}

int HttpNetworkTransaction::DoInitRequestBodyComplete(int result) {
  if (result == OK)
    next_state_ = STATE_BUILD_REQUEST;
  return result;
}

int HttpNetworkTransaction::DoBuildRequest() {
  next_state_ = STATE_BUILD_REQUEST_COMPLETE;
  headers_valid_ = false;

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_headers_.IsEmpty()) {
    bool using_proxy = (proxy_info_.is_http() || proxy_info_.is_https()) &&
                        !is_https_request();
    BuildRequestHeaders(using_proxy);
  }

  return OK;
}

int HttpNetworkTransaction::DoBuildRequestComplete(int result) {
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int HttpNetworkTransaction::DoSendRequest() {
  send_start_time_ = base::TimeTicks::Now();
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  return stream_->SendRequest(request_headers_, &response_, io_callback_);
}

int HttpNetworkTransaction::DoSendRequestComplete(int result) {
  send_end_time_ = base::TimeTicks::Now();
  if (result < 0)
    return HandleIOError(result);
  response_.network_accessed = true;
  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int HttpNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return stream_->ReadResponseHeaders(io_callback_);
}

int HttpNetworkTransaction::DoReadHeadersComplete(int result) {
  // We can get a certificate error or ERR_SSL_CLIENT_AUTH_CERT_NEEDED here
  // due to SSL renegotiation.
  if (IsCertificateError(result)) {
    // We don't handle a certificate error during SSL renegotiation, so we
    // have to return an error that's not in the certificate error range
    // (-2xx).
    LOG(ERROR) << "Got a server certificate with error " << result
               << " during SSL renegotiation";
    result = ERR_CERT_ERROR_IN_SSL_RENEGOTIATION;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    // TODO(wtc): Need a test case for this code path!
    DCHECK(stream_.get());
    DCHECK(is_https_request());
    response_.cert_request_info = new SSLCertRequestInfo;
    stream_->GetSSLCertRequestInfo(response_.cert_request_info.get());
    result = HandleCertificateRequest(result);
    if (result == OK)
      return result;
  }

  if (result == ERR_QUIC_HANDSHAKE_FAILED) {
    ResetConnectionAndRequestForResend();
    return OK;
  }

  // After we call RestartWithAuth a new response_time will be recorded, and
  // we need to be cautious about incorrectly logging the duration across the
  // authentication activity.
  if (result == OK)
    LogTransactionConnectedMetrics();

  // ERR_CONNECTION_CLOSED is treated differently at this point; if partial
  // response headers were received, we do the best we can to make sense of it
  // and send it back up the stack.
  //
  // TODO(davidben): Consider moving this to HttpBasicStream, It's a little
  // bizarre for SPDY. Assuming this logic is useful at all.
  // TODO(davidben): Bubble the error code up so we do not cache?
  if (result == ERR_CONNECTION_CLOSED && response_.headers.get())
    result = OK;

  if (result < 0)
    return HandleIOError(result);

  DCHECK(response_.headers.get());

  // On a 408 response from the server ("Request Timeout") on a stale socket,
  // retry the request.
  // Headers can be NULL because of http://crbug.com/384554.
  if (response_.headers.get() && response_.headers->response_code() == 408 &&
      stream_->IsConnectionReused()) {
    net_log_.AddEventWithNetErrorCode(
        NetLog::TYPE_HTTP_TRANSACTION_RESTART_AFTER_ERROR,
        response_.headers->response_code());
    // This will close the socket - it would be weird to try and reuse it, even
    // if the server doesn't actually close it.
    ResetConnectionAndRequestForResend();
    return OK;
  }

  // Like Net.HttpResponseCode, but only for MAIN_FRAME loads.
  if (request_->load_flags & LOAD_MAIN_FRAME) {
    const int response_code = response_.headers->response_code();
    UMA_HISTOGRAM_ENUMERATION(
        "Net.HttpResponseCode_Nxx_MainFrame", response_code/100, 10);
  }

  net_log_.AddEvent(
      NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
      base::Bind(&HttpResponseHeaders::NetLogCallback, response_.headers));

  if (response_.headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // HTTP/0.9 doesn't support the PUT method, so lack of response headers
    // indicates a buggy server.  See:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=193921
    if (request_->method == "PUT")
      return ERR_METHOD_NOT_SUPPORTED;
  }

  // Check for an intermediate 100 Continue response.  An origin server is
  // allowed to send this response even if we didn't ask for it, so we just
  // need to skip over it.
  // We treat any other 1xx in this same way (although in practice getting
  // a 1xx that isn't a 100 is rare).
  // Unless this is a WebSocket request, in which case we pass it on up.
  if (response_.headers->response_code() / 100 == 1 &&
      !ForWebSocketHandshake()) {
    response_.headers = new HttpResponseHeaders(std::string());
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  HostPortPair endpoint = HostPortPair(request_->url.HostNoBrackets(),
                                       request_->url.EffectiveIntPort());
  ProcessAlternateProtocol(session_,
                           *response_.headers.get(),
                           endpoint);

  int rv = HandleAuthChallenge();
  if (rv != OK)
    return rv;

  if (is_https_request())
    stream_->GetSSLInfo(&response_.ssl_info);

  headers_valid_ = true;

  if (session_->huffman_aggregator()) {
    session_->huffman_aggregator()->AggregateTransactionCharacterCounts(
        *request_,
        request_headers_,
        proxy_info_.proxy_server(),
        *response_.headers);
  }
  return OK;
}

int HttpNetworkTransaction::DoReadBody() {
  DCHECK(read_buf_.get());
  DCHECK_GT(read_buf_len_, 0);
  DCHECK(stream_ != NULL);

  next_state_ = STATE_READ_BODY_COMPLETE;
  return stream_->ReadResponseBody(
      read_buf_.get(), read_buf_len_, io_callback_);
}

int HttpNetworkTransaction::DoReadBodyComplete(int result) {
  // We are done with the Read call.
  bool done = false;
  if (result <= 0) {
    DCHECK_NE(ERR_IO_PENDING, result);
    done = true;
  }

  bool keep_alive = false;
  if (stream_->IsResponseBodyComplete()) {
    // Note: Just because IsResponseBodyComplete is true, we're not
    // necessarily "done".  We're only "done" when it is the last
    // read on this HttpNetworkTransaction, which will be signified
    // by a zero-length read.
    // TODO(mbelshe): The keepalive property is really a property of
    //    the stream.  No need to compute it here just to pass back
    //    to the stream's Close function.
    // TODO(rtenneti): CanFindEndOfResponse should return false if there are no
    // ResponseHeaders.
    if (stream_->CanFindEndOfResponse()) {
      HttpResponseHeaders* headers = GetResponseHeaders();
      if (headers)
        keep_alive = headers->IsKeepAlive();
    }
  }

  // Clean up connection if we are done.
  if (done) {
    LogTransactionMetrics();
    stream_->Close(!keep_alive);
    // Note: we don't reset the stream here.  We've closed it, but we still
    // need it around so that callers can call methods such as
    // GetUploadProgress() and have them be meaningful.
    // TODO(mbelshe): This means we closed the stream here, and we close it
    // again in ~HttpNetworkTransaction.  Clean that up.

    // The next Read call will return 0 (EOF).
  }

  // Clear these to avoid leaving around old state.
  read_buf_ = NULL;
  read_buf_len_ = 0;

  return result;
}

int HttpNetworkTransaction::DoDrainBodyForAuthRestart() {
  // This method differs from DoReadBody only in the next_state_.  So we just
  // call DoReadBody and override the next_state_.  Perhaps there is a more
  // elegant way for these two methods to share code.
  int rv = DoReadBody();
  DCHECK(next_state_ == STATE_READ_BODY_COMPLETE);
  next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE;
  return rv;
}

// TODO(wtc): This method and the DoReadBodyComplete method are almost
// the same.  Figure out a good way for these two methods to share code.
int HttpNetworkTransaction::DoDrainBodyForAuthRestartComplete(int result) {
  // keep_alive defaults to true because the very reason we're draining the
  // response body is to reuse the connection for auth restart.
  bool done = false, keep_alive = true;
  if (result < 0) {
    // Error or closed connection while reading the socket.
    done = true;
    keep_alive = false;
  } else if (stream_->IsResponseBodyComplete()) {
    done = true;
  }

  if (done) {
    DidDrainBodyForAuthRestart(keep_alive);
  } else {
    // Keep draining.
    next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
  }

  return OK;
}

void HttpNetworkTransaction::LogTransactionConnectedMetrics() {
  if (logged_response_time_)
    return;

  logged_response_time_ = true;

  base::TimeDelta total_duration = response_.response_time - start_time_;

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Net.Transaction_Connected",
      total_duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);

  bool reused_socket = stream_->IsConnectionReused();
  if (!reused_socket) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.Transaction_Connected_New_b",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }

  // Currently, non-HIGHEST priority requests are frame or sub-frame resource
  // types.  This will change when we also prioritize certain subresources like
  // css, js, etc.
  if (priority_ != HIGHEST) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.Priority_High_Latency_b",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.Priority_Low_Latency_b",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }
}

void HttpNetworkTransaction::LogTransactionMetrics() const {
  base::TimeDelta duration = base::Time::Now() -
                             response_.request_time;
  if (60 < duration.InMinutes())
    return;

  base::TimeDelta total_duration = base::Time::Now() - start_time_;

  UMA_HISTOGRAM_CUSTOM_TIMES("Net.Transaction_Latency_b", duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10),
                             100);
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.Transaction_Latency_Total",
                             total_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10), 100);

  if (!stream_->IsConnectionReused()) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.Transaction_Latency_Total_New_Connection",
        total_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);
  }
}

int HttpNetworkTransaction::HandleCertificateRequest(int error) {
  // There are two paths through which the server can request a certificate
  // from us.  The first is during the initial handshake, the second is
  // during SSL renegotiation.
  //
  // In both cases, we want to close the connection before proceeding.
  // We do this for two reasons:
  //   First, we don't want to keep the connection to the server hung for a
  //   long time while the user selects a certificate.
  //   Second, even if we did keep the connection open, NSS has a bug where
  //   restarting the handshake for ClientAuth is currently broken.
  DCHECK_EQ(error, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);

  if (stream_.get()) {
    // Since we already have a stream, we're being called as part of SSL
    // renegotiation.
    DCHECK(!stream_request_.get());
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    stream_->Close(true);
    stream_.reset();
  }

  // The server is asking for a client certificate during the initial
  // handshake.
  stream_request_.reset();

  // If the user selected one of the certificates in client_certs or declined
  // to provide one for this server before, use the past decision
  // automatically.
  scoped_refptr<X509Certificate> client_cert;
  bool found_cached_cert = session_->ssl_client_auth_cache()->Lookup(
      response_.cert_request_info->host_and_port, &client_cert);
  if (!found_cached_cert)
    return error;

  // Check that the certificate selected is still a certificate the server
  // is likely to accept, based on the criteria supplied in the
  // CertificateRequest message.
  if (client_cert.get()) {
    const std::vector<std::string>& cert_authorities =
        response_.cert_request_info->cert_authorities;

    bool cert_still_valid = cert_authorities.empty() ||
        client_cert->IsIssuedByEncoded(cert_authorities);
    if (!cert_still_valid)
      return error;
  }

  // TODO(davidben): Add a unit test which covers this path; we need to be
  // able to send a legitimate certificate and also bypass/clear the
  // SSL session cache.
  SSLConfig* ssl_config = response_.cert_request_info->is_proxy ?
      &proxy_ssl_config_ : &server_ssl_config_;
  ssl_config->send_client_cert = true;
  ssl_config->client_cert = client_cert;
  next_state_ = STATE_CREATE_STREAM;
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  return OK;
}

void HttpNetworkTransaction::HandleClientAuthError(int error) {
  if (server_ssl_config_.send_client_cert &&
      (error == ERR_SSL_PROTOCOL_ERROR || IsClientCertificateError(error))) {
    session_->ssl_client_auth_cache()->Remove(
        HostPortPair::FromURL(request_->url));
  }
}

// TODO(rch): This does not correctly handle errors when an SSL proxy is
// being used, as all of the errors are handled as if they were generated
// by the endpoint host, request_->url, rather than considering if they were
// generated by the SSL proxy. http://crbug.com/69329
int HttpNetworkTransaction::HandleSSLHandshakeError(int error) {
  DCHECK(request_);
  HandleClientAuthError(error);

  bool should_fallback = false;
  uint16 version_max = server_ssl_config_.version_max;

  switch (error) {
    case ERR_SSL_PROTOCOL_ERROR:
    case ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
      if (version_max >= SSL_PROTOCOL_VERSION_TLS1 &&
          version_max > server_ssl_config_.version_min) {
        // This could be a TLS-intolerant server or a server that chose a
        // cipher suite defined only for higher protocol versions (such as
        // an SSL 3.0 server that chose a TLS-only cipher suite).  Fall
        // back to the next lower version and retry.
        // NOTE: if the SSLClientSocket class doesn't support TLS 1.1,
        // specifying TLS 1.1 in version_max will result in a TLS 1.0
        // handshake, so falling back from TLS 1.1 to TLS 1.0 will simply
        // repeat the TLS 1.0 handshake. To avoid this problem, the default
        // version_max should match the maximum protocol version supported
        // by the SSLClientSocket class.
        version_max--;

        // Fallback to the lower SSL version.
        // While SSL 3.0 fallback should be eliminated because of security
        // reasons, there is a high risk of breaking the servers if this is
        // done in general.
        should_fallback = true;
      }
      break;
    case ERR_SSL_BAD_RECORD_MAC_ALERT:
      if (version_max >= SSL_PROTOCOL_VERSION_TLS1_1 &&
          version_max > server_ssl_config_.version_min) {
        // Some broken SSL devices negotiate TLS 1.0 when sent a TLS 1.1 or
        // 1.2 ClientHello, but then return a bad_record_mac alert. See
        // crbug.com/260358. In order to make the fallback as minimal as
        // possible, this fallback is only triggered for >= TLS 1.1.
        version_max--;
        should_fallback = true;
      }
      break;
    case ERR_SSL_INAPPROPRIATE_FALLBACK:
      // The server told us that we should not have fallen back. A buggy server
      // could trigger ERR_SSL_INAPPROPRIATE_FALLBACK with the initial
      // connection. |fallback_error_code_| is initialised to
      // ERR_SSL_INAPPROPRIATE_FALLBACK to catch this case.
      error = fallback_error_code_;
      break;
  }

  if (should_fallback) {
    net_log_.AddEvent(
        NetLog::TYPE_SSL_VERSION_FALLBACK,
        base::Bind(&NetLogSSLVersionFallbackCallback,
                   &request_->url, error, server_ssl_config_.version_max,
                   version_max));
    fallback_error_code_ = error;
    server_ssl_config_.version_max = version_max;
    server_ssl_config_.version_fallback = true;
    ResetConnectionAndRequestForResend();
    error = OK;
  }

  return error;
}

// This method determines whether it is safe to resend the request after an
// IO error.  It can only be called in response to request header or body
// write errors or response header read errors.  It should not be used in
// other cases, such as a Connect error.
int HttpNetworkTransaction::HandleIOError(int error) {
  // Because the peer may request renegotiation with client authentication at
  // any time, check and handle client authentication errors.
  HandleClientAuthError(error);

  switch (error) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_ABORTED:
    // There can be a race between the socket pool checking checking whether a
    // socket is still connected, receiving the FIN, and sending/reading data
    // on a reused socket.  If we receive the FIN between the connectedness
    // check and writing/reading from the socket, we may first learn the socket
    // is disconnected when we get a ERR_SOCKET_NOT_CONNECTED.  This will most
    // likely happen when trying to retrieve its IP address.
    // See http://crbug.com/105824 for more details.
    case ERR_SOCKET_NOT_CONNECTED:
    // If a socket is closed on its initial request, HttpStreamParser returns
    // ERR_EMPTY_RESPONSE. This may still be close/reuse race if the socket was
    // preconnected but failed to be used before the server timed it out.
    case ERR_EMPTY_RESPONSE:
      if (ShouldResendRequest()) {
        net_log_.AddEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
    case ERR_SPDY_PING_FAILED:
    case ERR_SPDY_SERVER_REFUSED_STREAM:
    case ERR_QUIC_HANDSHAKE_FAILED:
      net_log_.AddEventWithNetErrorCode(
          NetLog::TYPE_HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      ResetConnectionAndRequestForResend();
      error = OK;
      break;
  }
  return error;
}

void HttpNetworkTransaction::ResetStateForRestart() {
  ResetStateForAuthRestart();
  if (stream_)
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
  stream_.reset();
}

void HttpNetworkTransaction::ResetStateForAuthRestart() {
  send_start_time_ = base::TimeTicks();
  send_end_time_ = base::TimeTicks();

  pending_auth_target_ = HttpAuth::AUTH_NONE;
  read_buf_ = NULL;
  read_buf_len_ = 0;
  headers_valid_ = false;
  request_headers_.Clear();
  response_ = HttpResponseInfo();
  establishing_tunnel_ = false;
}

HttpResponseHeaders* HttpNetworkTransaction::GetResponseHeaders() const {
  return response_.headers.get();
}

bool HttpNetworkTransaction::ShouldResendRequest() const {
  bool connection_is_proven = stream_->IsConnectionReused();
  bool has_received_headers = GetResponseHeaders() != NULL;

  // NOTE: we resend a request only if we reused a keep-alive connection.
  // This automatically prevents an infinite resend loop because we'll run
  // out of the cached keep-alive connections eventually.
  if (connection_is_proven && !has_received_headers)
    return true;
  return false;
}

void HttpNetworkTransaction::ResetConnectionAndRequestForResend() {
  if (stream_.get()) {
    stream_->Close(true);
    stream_.reset();
  }

  // We need to clear request_headers_ because it contains the real request
  // headers, but we may need to resend the CONNECT request first to recreate
  // the SSL tunnel.
  request_headers_.Clear();
  next_state_ = STATE_CREATE_STREAM;  // Resend the request.
}

bool HttpNetworkTransaction::ShouldApplyProxyAuth() const {
  return !is_https_request() &&
      (proxy_info_.is_https() || proxy_info_.is_http());
}

bool HttpNetworkTransaction::ShouldApplyServerAuth() const {
  return !(request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA);
}

int HttpNetworkTransaction::HandleAuthChallenge() {
  scoped_refptr<HttpResponseHeaders> headers(GetResponseHeaders());
  DCHECK(headers.get());

  int status = headers->response_code();
  if (status != HTTP_UNAUTHORIZED &&
      status != HTTP_PROXY_AUTHENTICATION_REQUIRED)
    return OK;
  HttpAuth::Target target = status == HTTP_PROXY_AUTHENTICATION_REQUIRED ?
                            HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;
  if (target == HttpAuth::AUTH_PROXY && proxy_info_.is_direct())
    return ERR_UNEXPECTED_PROXY_AUTH;

  // This case can trigger when an HTTPS server responds with a "Proxy
  // authentication required" status code through a non-authenticating
  // proxy.
  if (!auth_controllers_[target].get())
    return ERR_UNEXPECTED_PROXY_AUTH;

  int rv = auth_controllers_[target]->HandleAuthChallenge(
      headers, (request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA) != 0, false,
      net_log_);
  if (auth_controllers_[target]->HaveAuthHandler())
      pending_auth_target_ = target;

  scoped_refptr<AuthChallengeInfo> auth_info =
      auth_controllers_[target]->auth_info();
  if (auth_info.get())
      response_.auth_challenge = auth_info;

  return rv;
}

bool HttpNetworkTransaction::HaveAuth(HttpAuth::Target target) const {
  return auth_controllers_[target].get() &&
      auth_controllers_[target]->HaveAuth();
}

GURL HttpNetworkTransaction::AuthURL(HttpAuth::Target target) const {
  switch (target) {
    case HttpAuth::AUTH_PROXY: {
      if (!proxy_info_.proxy_server().is_valid() ||
          proxy_info_.proxy_server().is_direct()) {
        return GURL();  // There is no proxy server.
      }
      const char* scheme = proxy_info_.is_https() ? "https://" : "http://";
      return GURL(scheme +
                  proxy_info_.proxy_server().host_port_pair().ToString());
    }
    case HttpAuth::AUTH_SERVER:
      return request_->url;
    default:
     return GURL();
  }
}

bool HttpNetworkTransaction::ForWebSocketHandshake() const {
  return websocket_handshake_stream_base_create_helper_ &&
         request_->url.SchemeIsWSOrWSS();
}

#define STATE_CASE(s) \
  case s: \
    description = base::StringPrintf("%s (0x%08X)", #s, s); \
    break

std::string HttpNetworkTransaction::DescribeState(State state) {
  std::string description;
  switch (state) {
    STATE_CASE(STATE_NOTIFY_BEFORE_CREATE_STREAM);
    STATE_CASE(STATE_CREATE_STREAM);
    STATE_CASE(STATE_CREATE_STREAM_COMPLETE);
    STATE_CASE(STATE_INIT_REQUEST_BODY);
    STATE_CASE(STATE_INIT_REQUEST_BODY_COMPLETE);
    STATE_CASE(STATE_BUILD_REQUEST);
    STATE_CASE(STATE_BUILD_REQUEST_COMPLETE);
    STATE_CASE(STATE_SEND_REQUEST);
    STATE_CASE(STATE_SEND_REQUEST_COMPLETE);
    STATE_CASE(STATE_READ_HEADERS);
    STATE_CASE(STATE_READ_HEADERS_COMPLETE);
    STATE_CASE(STATE_READ_BODY);
    STATE_CASE(STATE_READ_BODY_COMPLETE);
    STATE_CASE(STATE_DRAIN_BODY_FOR_AUTH_RESTART);
    STATE_CASE(STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE);
    STATE_CASE(STATE_NONE);
    default:
      description = base::StringPrintf("Unknown state 0x%08X (%u)", state,
                                       state);
      break;
  }
  return description;
}

#undef STATE_CASE

}  // namespace net
