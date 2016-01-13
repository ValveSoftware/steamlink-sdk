// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_job.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/quic/quic_http_stream.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

// Returns parameters associated with the start of a HTTP stream job.
base::Value* NetLogHttpStreamJobCallback(const GURL* original_url,
                                         const GURL* url,
                                         RequestPriority priority,
                                         NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("original_url", original_url->GetOrigin().spec());
  dict->SetString("url", url->GetOrigin().spec());
  dict->SetString("priority", RequestPriorityToString(priority));
  return dict;
}

// Returns parameters associated with the Proto (with NPN negotiation) of a HTTP
// stream.
base::Value* NetLogHttpStreamProtoCallback(
    const SSLClientSocket::NextProtoStatus status,
    const std::string* proto,
    const std::string* server_protos,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("next_proto_status",
                  SSLClientSocket::NextProtoStatusToString(status));
  dict->SetString("proto", *proto);
  dict->SetString("server_protos",
                  SSLClientSocket::ServerProtosToString(*server_protos));
  return dict;
}

HttpStreamFactoryImpl::Job::Job(HttpStreamFactoryImpl* stream_factory,
                                HttpNetworkSession* session,
                                const HttpRequestInfo& request_info,
                                RequestPriority priority,
                                const SSLConfig& server_ssl_config,
                                const SSLConfig& proxy_ssl_config,
                                NetLog* net_log)
    : request_(NULL),
      request_info_(request_info),
      priority_(priority),
      server_ssl_config_(server_ssl_config),
      proxy_ssl_config_(proxy_ssl_config),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_HTTP_STREAM_JOB)),
      io_callback_(base::Bind(&Job::OnIOComplete, base::Unretained(this))),
      connection_(new ClientSocketHandle),
      session_(session),
      stream_factory_(stream_factory),
      next_state_(STATE_NONE),
      pac_request_(NULL),
      blocking_job_(NULL),
      waiting_job_(NULL),
      using_ssl_(false),
      using_spdy_(false),
      using_quic_(false),
      quic_request_(session_->quic_stream_factory()),
      using_existing_quic_session_(false),
      spdy_certificate_error_(OK),
      establishing_tunnel_(false),
      was_npn_negotiated_(false),
      protocol_negotiated_(kProtoUnknown),
      num_streams_(0),
      spdy_session_direct_(false),
      job_status_(STATUS_RUNNING),
      other_job_status_(STATUS_RUNNING),
      ptr_factory_(this) {
  DCHECK(stream_factory);
  DCHECK(session);
}

HttpStreamFactoryImpl::Job::~Job() {
  net_log_.EndEvent(NetLog::TYPE_HTTP_STREAM_JOB);

  // When we're in a partially constructed state, waiting for the user to
  // provide certificate handling information or authentication, we can't reuse
  // this stream at all.
  if (next_state_ == STATE_WAITING_USER_ACTION) {
    connection_->socket()->Disconnect();
    connection_.reset();
  }

  if (pac_request_)
    session_->proxy_service()->CancelPacRequest(pac_request_);

  // The stream could be in a partial state.  It is not reusable.
  if (stream_.get() && next_state_ != STATE_DONE)
    stream_->Close(true /* not reusable */);
}

void HttpStreamFactoryImpl::Job::Start(Request* request) {
  DCHECK(request);
  request_ = request;
  StartInternal();
}

int HttpStreamFactoryImpl::Job::Preconnect(int num_streams) {
  DCHECK_GT(num_streams, 0);
  HostPortPair origin_server =
      HostPortPair(request_info_.url.HostNoBrackets(),
                   request_info_.url.EffectiveIntPort());
  base::WeakPtr<HttpServerProperties> http_server_properties =
      session_->http_server_properties();
  if (http_server_properties &&
      http_server_properties->SupportsSpdy(origin_server)) {
    num_streams_ = 1;
  } else {
    num_streams_ = num_streams;
  }
  return StartInternal();
}

int HttpStreamFactoryImpl::Job::RestartTunnelWithProxyAuth(
    const AuthCredentials& credentials) {
  DCHECK(establishing_tunnel_);
  next_state_ = STATE_RESTART_TUNNEL_AUTH;
  stream_.reset();
  return RunLoop(OK);
}

LoadState HttpStreamFactoryImpl::Job::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_PROXY_COMPLETE:
      return session_->proxy_service()->GetLoadState(pac_request_);
    case STATE_INIT_CONNECTION_COMPLETE:
    case STATE_CREATE_STREAM_COMPLETE:
      return using_quic_ ? LOAD_STATE_CONNECTING : connection_->GetLoadState();
    default:
      return LOAD_STATE_IDLE;
  }
}

void HttpStreamFactoryImpl::Job::MarkAsAlternate(
    const GURL& original_url,
    PortAlternateProtocolPair alternate) {
  DCHECK(!original_url_.get());
  original_url_.reset(new GURL(original_url));
  if (alternate.protocol == QUIC) {
    DCHECK(session_->params().enable_quic);
    using_quic_ = true;
  }
}

void HttpStreamFactoryImpl::Job::WaitFor(Job* job) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK_EQ(STATE_NONE, job->next_state_);
  DCHECK(!blocking_job_);
  DCHECK(!job->waiting_job_);
  blocking_job_ = job;
  job->waiting_job_ = this;
}

void HttpStreamFactoryImpl::Job::Resume(Job* job) {
  DCHECK_EQ(blocking_job_, job);
  blocking_job_ = NULL;

  // We know we're blocked if the next_state_ is STATE_WAIT_FOR_JOB_COMPLETE.
  // Unblock |this|.
  if (next_state_ == STATE_WAIT_FOR_JOB_COMPLETE) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HttpStreamFactoryImpl::Job::OnIOComplete,
                   ptr_factory_.GetWeakPtr(), OK));
  }
}

void HttpStreamFactoryImpl::Job::Orphan(const Request* request) {
  DCHECK_EQ(request_, request);
  request_ = NULL;
  if (blocking_job_) {
    // We've been orphaned, but there's a job we're blocked on. Don't bother
    // racing, just cancel ourself.
    DCHECK(blocking_job_->waiting_job_);
    blocking_job_->waiting_job_ = NULL;
    blocking_job_ = NULL;
    if (stream_factory_->for_websockets_ &&
        connection_ && connection_->socket()) {
      connection_->socket()->Disconnect();
    }
    stream_factory_->OnOrphanedJobComplete(this);
  } else if (stream_factory_->for_websockets_) {
    // We cancel this job because a WebSocketHandshakeStream can't be created
    // without a WebSocketHandshakeStreamBase::CreateHelper which is stored in
    // the Request class and isn't accessible from this job.
    if (connection_ && connection_->socket()) {
      connection_->socket()->Disconnect();
    }
    stream_factory_->OnOrphanedJobComplete(this);
  }
}

void HttpStreamFactoryImpl::Job::SetPriority(RequestPriority priority) {
  priority_ = priority;
  // TODO(akalin): Propagate this to |connection_| and maybe the
  // preconnect state.
}

bool HttpStreamFactoryImpl::Job::was_npn_negotiated() const {
  return was_npn_negotiated_;
}

NextProto HttpStreamFactoryImpl::Job::protocol_negotiated() const {
  return protocol_negotiated_;
}

bool HttpStreamFactoryImpl::Job::using_spdy() const {
  return using_spdy_;
}

const SSLConfig& HttpStreamFactoryImpl::Job::server_ssl_config() const {
  return server_ssl_config_;
}

const SSLConfig& HttpStreamFactoryImpl::Job::proxy_ssl_config() const {
  return proxy_ssl_config_;
}

const ProxyInfo& HttpStreamFactoryImpl::Job::proxy_info() const {
  return proxy_info_;
}

void HttpStreamFactoryImpl::Job::GetSSLInfo() {
  DCHECK(using_ssl_);
  DCHECK(!establishing_tunnel_);
  DCHECK(connection_.get() && connection_->socket());
  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  ssl_socket->GetSSLInfo(&ssl_info_);
}

SpdySessionKey HttpStreamFactoryImpl::Job::GetSpdySessionKey() const {
  // In the case that we're using an HTTPS proxy for an HTTP url,
  // we look for a SPDY session *to* the proxy, instead of to the
  // origin server.
  PrivacyMode privacy_mode = request_info_.privacy_mode;
  if (IsHttpsProxyAndHttpUrl()) {
    return SpdySessionKey(proxy_info_.proxy_server().host_port_pair(),
                          ProxyServer::Direct(),
                          privacy_mode);
  } else {
    return SpdySessionKey(origin_,
                          proxy_info_.proxy_server(),
                          privacy_mode);
  }
}

bool HttpStreamFactoryImpl::Job::CanUseExistingSpdySession() const {
  // We need to make sure that if a spdy session was created for
  // https://somehost/ that we don't use that session for http://somehost:443/.
  // The only time we can use an existing session is if the request URL is
  // https (the normal case) or if we're connection to a SPDY proxy, or
  // if we're running with force_spdy_always_.  crbug.com/133176
  // TODO(ricea): Add "wss" back to this list when SPDY WebSocket support is
  // working.
  return request_info_.url.SchemeIs("https") ||
         proxy_info_.proxy_server().is_https() ||
         session_->params().force_spdy_always;
}

void HttpStreamFactoryImpl::Job::OnStreamReadyCallback() {
  DCHECK(stream_.get());
  DCHECK(!IsPreconnecting());
  DCHECK(!stream_factory_->for_websockets_);
  if (IsOrphaned()) {
    stream_factory_->OnOrphanedJobComplete(this);
  } else {
    request_->Complete(was_npn_negotiated(),
                       protocol_negotiated(),
                       using_spdy(),
                       net_log_);
    request_->OnStreamReady(this, server_ssl_config_, proxy_info_,
                            stream_.release());
  }
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnWebSocketHandshakeStreamReadyCallback() {
  DCHECK(websocket_stream_);
  DCHECK(!IsPreconnecting());
  DCHECK(stream_factory_->for_websockets_);
  // An orphaned WebSocket job will be closed immediately and
  // never be ready.
  DCHECK(!IsOrphaned());
  request_->Complete(was_npn_negotiated(),
                     protocol_negotiated(),
                     using_spdy(),
                     net_log_);
  request_->OnWebSocketHandshakeStreamReady(this,
                                            server_ssl_config_,
                                            proxy_info_,
                                            websocket_stream_.release());
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNewSpdySessionReadyCallback() {
  DCHECK(stream_.get());
  DCHECK(!IsPreconnecting());
  DCHECK(using_spdy());
  // Note: an event loop iteration has passed, so |new_spdy_session_| may be
  // NULL at this point if the SpdySession closed immediately after creation.
  base::WeakPtr<SpdySession> spdy_session = new_spdy_session_;
  new_spdy_session_.reset();

  // TODO(jgraettinger): Notify the factory, and let that notify |request_|,
  // rather than notifying |request_| directly.
  if (IsOrphaned()) {
    if (spdy_session) {
      stream_factory_->OnNewSpdySessionReady(
          spdy_session, spdy_session_direct_, server_ssl_config_, proxy_info_,
          was_npn_negotiated(), protocol_negotiated(), using_spdy(), net_log_);
    }
    stream_factory_->OnOrphanedJobComplete(this);
  } else {
    request_->OnNewSpdySessionReady(
        this, stream_.Pass(), spdy_session, spdy_session_direct_);
  }
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnStreamFailedCallback(int result) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnStreamFailed(this, result, server_ssl_config_);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnCertificateErrorCallback(
    int result, const SSLInfo& ssl_info) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnCertificateError(this, result, server_ssl_config_, ssl_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNeedsProxyAuthCallback(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnNeedsProxyAuth(
        this, response, server_ssl_config_, proxy_info_, auth_controller);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNeedsClientAuthCallback(
    SSLCertRequestInfo* cert_info) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnNeedsClientAuth(this, server_ssl_config_, cert_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnHttpsProxyTunnelResponseCallback(
    const HttpResponseInfo& response_info,
    HttpStream* stream) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnHttpsProxyTunnelResponse(
        this, response_info, server_ssl_config_, proxy_info_, stream);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnPreconnectsComplete() {
  DCHECK(!request_);
  if (new_spdy_session_.get()) {
    stream_factory_->OnNewSpdySessionReady(new_spdy_session_,
                                           spdy_session_direct_,
                                           server_ssl_config_,
                                           proxy_info_,
                                           was_npn_negotiated(),
                                           protocol_negotiated(),
                                           using_spdy(),
                                           net_log_);
  }
  stream_factory_->OnPreconnectsComplete(this);
  // |this| may be deleted after this call.
}

// static
int HttpStreamFactoryImpl::Job::OnHostResolution(
    SpdySessionPool* spdy_session_pool,
    const SpdySessionKey& spdy_session_key,
    const AddressList& addresses,
    const BoundNetLog& net_log) {
  // It is OK to dereference spdy_session_pool, because the
  // ClientSocketPoolManager will be destroyed in the same callback that
  // destroys the SpdySessionPool.
  return
      spdy_session_pool->FindAvailableSession(spdy_session_key, net_log) ?
      ERR_SPDY_SESSION_ALREADY_EXISTS : OK;
}

void HttpStreamFactoryImpl::Job::OnIOComplete(int result) {
  RunLoop(result);
}

int HttpStreamFactoryImpl::Job::RunLoop(int result) {
  result = DoLoop(result);

  if (result == ERR_IO_PENDING)
    return result;

  // If there was an error, we should have already resumed the |waiting_job_|,
  // if there was one.
  DCHECK(result == OK || waiting_job_ == NULL);

  if (IsPreconnecting()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HttpStreamFactoryImpl::Job::OnPreconnectsComplete,
                   ptr_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  }

  if (IsCertificateError(result)) {
    // Retrieve SSL information from the socket.
    GetSSLInfo();

    next_state_ = STATE_WAITING_USER_ACTION;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HttpStreamFactoryImpl::Job::OnCertificateErrorCallback,
                   ptr_factory_.GetWeakPtr(), result, ssl_info_));
    return ERR_IO_PENDING;
  }

  switch (result) {
    case ERR_PROXY_AUTH_REQUESTED: {
      UMA_HISTOGRAM_BOOLEAN("Net.ProxyAuthRequested.HasConnection",
                            connection_.get() != NULL);
      if (!connection_.get())
        return ERR_PROXY_AUTH_REQUESTED_WITH_NO_CONNECTION;
      CHECK(connection_->socket());
      CHECK(establishing_tunnel_);

      next_state_ = STATE_WAITING_USER_ACTION;
      ProxyClientSocket* proxy_socket =
          static_cast<ProxyClientSocket*>(connection_->socket());
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&Job::OnNeedsProxyAuthCallback, ptr_factory_.GetWeakPtr(),
                     *proxy_socket->GetConnectResponseInfo(),
                     proxy_socket->GetAuthController()));
      return ERR_IO_PENDING;
    }

    case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&Job::OnNeedsClientAuthCallback, ptr_factory_.GetWeakPtr(),
                     connection_->ssl_error_response_info().cert_request_info));
      return ERR_IO_PENDING;

    case ERR_HTTPS_PROXY_TUNNEL_RESPONSE: {
      DCHECK(connection_.get());
      DCHECK(connection_->socket());
      DCHECK(establishing_tunnel_);

      ProxyClientSocket* proxy_socket =
          static_cast<ProxyClientSocket*>(connection_->socket());
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&Job::OnHttpsProxyTunnelResponseCallback,
                     ptr_factory_.GetWeakPtr(),
                     *proxy_socket->GetConnectResponseInfo(),
                     proxy_socket->CreateConnectResponseStream()));
      return ERR_IO_PENDING;
    }

    case OK:
      job_status_ = STATUS_SUCCEEDED;
      MaybeMarkAlternateProtocolBroken();
      next_state_ = STATE_DONE;
      if (new_spdy_session_.get()) {
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&Job::OnNewSpdySessionReadyCallback,
                       ptr_factory_.GetWeakPtr()));
      } else if (stream_factory_->for_websockets_) {
        DCHECK(websocket_stream_);
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&Job::OnWebSocketHandshakeStreamReadyCallback,
                       ptr_factory_.GetWeakPtr()));
      } else {
        DCHECK(stream_.get());
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&Job::OnStreamReadyCallback, ptr_factory_.GetWeakPtr()));
      }
      return ERR_IO_PENDING;

    default:
      if (job_status_ != STATUS_BROKEN) {
        DCHECK_EQ(STATUS_RUNNING, job_status_);
        job_status_ = STATUS_FAILED;
        MaybeMarkAlternateProtocolBroken();
      }
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&Job::OnStreamFailedCallback, ptr_factory_.GetWeakPtr(),
                     result));
      return ERR_IO_PENDING;
  }
}

int HttpStreamFactoryImpl::Job::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_START:
        DCHECK_EQ(OK, rv);
        rv = DoStart();
        break;
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, rv);
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
        break;
      case STATE_WAIT_FOR_JOB:
        DCHECK_EQ(OK, rv);
        rv = DoWaitForJob();
        break;
      case STATE_WAIT_FOR_JOB_COMPLETE:
        rv = DoWaitForJobComplete(rv);
        break;
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_WAITING_USER_ACTION:
        rv = DoWaitingUserAction(rv);
        break;
      case STATE_RESTART_TUNNEL_AUTH:
        DCHECK_EQ(OK, rv);
        rv = DoRestartTunnelAuth();
        break;
      case STATE_RESTART_TUNNEL_AUTH_COMPLETE:
        rv = DoRestartTunnelAuthComplete(rv);
        break;
      case STATE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case STATE_CREATE_STREAM_COMPLETE:
        rv = DoCreateStreamComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int HttpStreamFactoryImpl::Job::StartInternal() {
  CHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_START;
  int rv = RunLoop(OK);
  DCHECK_EQ(ERR_IO_PENDING, rv);
  return rv;
}

int HttpStreamFactoryImpl::Job::DoStart() {
  int port = request_info_.url.EffectiveIntPort();
  origin_ = HostPortPair(request_info_.url.HostNoBrackets(), port);
  origin_url_ = stream_factory_->ApplyHostMappingRules(
      request_info_.url, &origin_);

  net_log_.BeginEvent(NetLog::TYPE_HTTP_STREAM_JOB,
                      base::Bind(&NetLogHttpStreamJobCallback,
                                 &request_info_.url, &origin_url_,
                                 priority_));

  // Don't connect to restricted ports.
  bool is_port_allowed = IsPortAllowedByDefault(port);
  if (request_info_.url.SchemeIs("ftp")) {
    // Never share connection with other jobs for FTP requests.
    DCHECK(!waiting_job_);

    is_port_allowed = IsPortAllowedByFtp(port);
  }
  if (!is_port_allowed && !IsPortAllowedByOverride(port)) {
    if (waiting_job_) {
      waiting_job_->Resume(this);
      waiting_job_ = NULL;
    }
    return ERR_UNSAFE_PORT;
  }

  next_state_ = STATE_RESOLVE_PROXY;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoResolveProxy() {
  DCHECK(!pac_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  return session_->proxy_service()->ResolveProxy(
      request_info_.url, &proxy_info_, io_callback_, &pac_request_, net_log_);
}

int HttpStreamFactoryImpl::Job::DoResolveProxyComplete(int result) {
  pac_request_ = NULL;

  if (result == OK) {
    // Remove unsupported proxies from the list.
    proxy_info_.RemoveProxiesWithoutScheme(
        ProxyServer::SCHEME_DIRECT | ProxyServer::SCHEME_QUIC |
        ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS |
        ProxyServer::SCHEME_SOCKS4 | ProxyServer::SCHEME_SOCKS5);

    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      result = ERR_NO_SUPPORTED_PROXIES;
    } else if (using_quic_ &&
               (!proxy_info_.is_quic() && !proxy_info_.is_direct())) {
      // QUIC can not be spoken to non-QUIC proxies.  This error should not be
      // user visible, because the non-alternate job should be resumed.
      result = ERR_NO_SUPPORTED_PROXIES;
    }
  }

  if (result != OK) {
    if (waiting_job_) {
      waiting_job_->Resume(this);
      waiting_job_ = NULL;
    }
    return result;
  }

  if (blocking_job_)
    next_state_ = STATE_WAIT_FOR_JOB;
  else
    next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

bool HttpStreamFactoryImpl::Job::ShouldForceSpdySSL() const {
  bool rv = session_->params().force_spdy_always &&
      session_->params().force_spdy_over_ssl;
  return rv && !session_->HasSpdyExclusion(origin_);
}

bool HttpStreamFactoryImpl::Job::ShouldForceSpdyWithoutSSL() const {
  bool rv = session_->params().force_spdy_always &&
      !session_->params().force_spdy_over_ssl;
  return rv && !session_->HasSpdyExclusion(origin_);
}

bool HttpStreamFactoryImpl::Job::ShouldForceQuic() const {
  return session_->params().enable_quic &&
    session_->params().origin_to_force_quic_on.Equals(origin_) &&
    proxy_info_.is_direct();
}

int HttpStreamFactoryImpl::Job::DoWaitForJob() {
  DCHECK(blocking_job_);
  next_state_ = STATE_WAIT_FOR_JOB_COMPLETE;
  return ERR_IO_PENDING;
}

int HttpStreamFactoryImpl::Job::DoWaitForJobComplete(int result) {
  DCHECK(!blocking_job_);
  DCHECK_EQ(OK, result);
  next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoInitConnection() {
  DCHECK(!blocking_job_);
  DCHECK(!connection_->is_initialized());
  DCHECK(proxy_info_.proxy_server().is_valid());
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  using_ssl_ = request_info_.url.SchemeIs("https") ||
      request_info_.url.SchemeIs("wss") || ShouldForceSpdySSL();
  using_spdy_ = false;

  if (ShouldForceQuic())
    using_quic_ = true;

  if (proxy_info_.is_quic())
    using_quic_ = true;

  if (using_quic_) {
    DCHECK(session_->params().enable_quic);
    if (proxy_info_.is_quic() && !request_info_.url.SchemeIs("http")) {
      NOTREACHED();
      // TODO(rch): support QUIC proxies for HTTPS urls.
      return ERR_NOT_IMPLEMENTED;
    }
    HostPortPair destination = proxy_info_.is_quic() ?
        proxy_info_.proxy_server().host_port_pair() : origin_;
    next_state_ = STATE_INIT_CONNECTION_COMPLETE;
    bool secure_quic = using_ssl_ || proxy_info_.is_quic();
    int rv = quic_request_.Request(
        destination, secure_quic, request_info_.privacy_mode,
        request_info_.method, net_log_, io_callback_);
    if (rv == OK) {
      using_existing_quic_session_ = true;
    } else {
      // OK, there's no available QUIC session. Let |waiting_job_| resume
      // if it's paused.
      if (waiting_job_) {
        waiting_job_->Resume(this);
        waiting_job_ = NULL;
      }
    }
    return rv;
  }

  // Check first if we have a spdy session for this group.  If so, then go
  // straight to using that.
  SpdySessionKey spdy_session_key = GetSpdySessionKey();
  base::WeakPtr<SpdySession> spdy_session =
      session_->spdy_session_pool()->FindAvailableSession(
          spdy_session_key, net_log_);
  if (spdy_session && CanUseExistingSpdySession()) {
    // If we're preconnecting, but we already have a SpdySession, we don't
    // actually need to preconnect any sockets, so we're done.
    if (IsPreconnecting())
      return OK;
    using_spdy_ = true;
    next_state_ = STATE_CREATE_STREAM;
    existing_spdy_session_ = spdy_session;
    return OK;
  } else if (request_ && !request_->HasSpdySessionKey() &&
             (using_ssl_ || ShouldForceSpdyWithoutSSL())) {
    // Update the spdy session key for the request that launched this job.
    request_->SetSpdySessionKey(spdy_session_key);
  }

  // OK, there's no available SPDY session. Let |waiting_job_| resume if it's
  // paused.

  if (waiting_job_) {
    waiting_job_->Resume(this);
    waiting_job_ = NULL;
  }

  if (proxy_info_.is_http() || proxy_info_.is_https())
    establishing_tunnel_ = using_ssl_;

  bool want_spdy_over_npn = original_url_ != NULL;

  if (proxy_info_.is_https()) {
    InitSSLConfig(proxy_info_.proxy_server().host_port_pair(),
                  &proxy_ssl_config_,
                  true /* is a proxy server */);
    // Disable revocation checking for HTTPS proxies since the revocation
    // requests are probably going to need to go through the proxy too.
    proxy_ssl_config_.rev_checking_enabled = false;
  }
  if (using_ssl_) {
    InitSSLConfig(origin_, &server_ssl_config_,
                  false /* not a proxy server */);
  }

  if (IsPreconnecting()) {
    DCHECK(!stream_factory_->for_websockets_);
    return PreconnectSocketsForHttpRequest(
        origin_url_,
        request_info_.extra_headers,
        request_info_.load_flags,
        priority_,
        session_,
        proxy_info_,
        ShouldForceSpdySSL(),
        want_spdy_over_npn,
        server_ssl_config_,
        proxy_ssl_config_,
        request_info_.privacy_mode,
        net_log_,
        num_streams_);
  }

  // If we can't use a SPDY session, don't both checking for one after
  // the hostname is resolved.
  OnHostResolutionCallback resolution_callback = CanUseExistingSpdySession() ?
      base::Bind(&Job::OnHostResolution, session_->spdy_session_pool(),
                 GetSpdySessionKey()) :
      OnHostResolutionCallback();
  if (stream_factory_->for_websockets_) {
    // TODO(ricea): Re-enable NPN when WebSockets over SPDY is supported.
    SSLConfig websocket_server_ssl_config = server_ssl_config_;
    websocket_server_ssl_config.next_protos.clear();
    return InitSocketHandleForWebSocketRequest(
        origin_url_, request_info_.extra_headers, request_info_.load_flags,
        priority_, session_, proxy_info_, ShouldForceSpdySSL(),
        want_spdy_over_npn, websocket_server_ssl_config, proxy_ssl_config_,
        request_info_.privacy_mode, net_log_,
        connection_.get(), resolution_callback, io_callback_);
  }

  return InitSocketHandleForHttpRequest(
      origin_url_, request_info_.extra_headers, request_info_.load_flags,
      priority_, session_, proxy_info_, ShouldForceSpdySSL(),
      want_spdy_over_npn, server_ssl_config_, proxy_ssl_config_,
      request_info_.privacy_mode, net_log_,
      connection_.get(), resolution_callback, io_callback_);
}

int HttpStreamFactoryImpl::Job::DoInitConnectionComplete(int result) {
  if (IsPreconnecting()) {
    if (using_quic_)
      return result;
    DCHECK_EQ(OK, result);
    return OK;
  }

  if (result == ERR_SPDY_SESSION_ALREADY_EXISTS) {
    // We found a SPDY connection after resolving the host.  This is
    // probably an IP pooled connection.
    SpdySessionKey spdy_session_key = GetSpdySessionKey();
    existing_spdy_session_ =
        session_->spdy_session_pool()->FindAvailableSession(
            spdy_session_key, net_log_);
    if (existing_spdy_session_) {
      using_spdy_ = true;
      next_state_ = STATE_CREATE_STREAM;
    } else {
      // It is possible that the spdy session no longer exists.
      ReturnToStateInitConnection(true /* close connection */);
    }
    return OK;
  }

  // TODO(willchan): Make this a bit more exact. Maybe there are recoverable
  // errors, such as ignoring certificate errors for Alternate-Protocol.
  if (result < 0 && waiting_job_) {
    waiting_job_->Resume(this);
    waiting_job_ = NULL;
  }

  // |result| may be the result of any of the stacked pools. The following
  // logic is used when determining how to interpret an error.
  // If |result| < 0:
  //   and connection_->socket() != NULL, then the SSL handshake ran and it
  //     is a potentially recoverable error.
  //   and connection_->socket == NULL and connection_->is_ssl_error() is true,
  //     then the SSL handshake ran with an unrecoverable error.
  //   otherwise, the error came from one of the other pools.
  bool ssl_started = using_ssl_ && (result == OK || connection_->socket() ||
                                    connection_->is_ssl_error());

  if (ssl_started && (result == OK || IsCertificateError(result))) {
    if (using_quic_ && result == OK) {
      was_npn_negotiated_ = true;
      NextProto protocol_negotiated =
          SSLClientSocket::NextProtoFromString("quic/1+spdy/3");
      protocol_negotiated_ = protocol_negotiated;
    } else {
      SSLClientSocket* ssl_socket =
          static_cast<SSLClientSocket*>(connection_->socket());
      if (ssl_socket->WasNpnNegotiated()) {
        was_npn_negotiated_ = true;
        std::string proto;
        std::string server_protos;
        SSLClientSocket::NextProtoStatus status =
            ssl_socket->GetNextProto(&proto, &server_protos);
        NextProto protocol_negotiated =
            SSLClientSocket::NextProtoFromString(proto);
        protocol_negotiated_ = protocol_negotiated;
        net_log_.AddEvent(
            NetLog::TYPE_HTTP_STREAM_REQUEST_PROTO,
            base::Bind(&NetLogHttpStreamProtoCallback,
                       status, &proto, &server_protos));
        if (ssl_socket->was_spdy_negotiated())
          SwitchToSpdyMode();
      }
      if (ShouldForceSpdySSL())
        SwitchToSpdyMode();
    }
  } else if (proxy_info_.is_https() && connection_->socket() &&
        result == OK) {
    ProxyClientSocket* proxy_socket =
      static_cast<ProxyClientSocket*>(connection_->socket());
    if (proxy_socket->IsUsingSpdy()) {
      was_npn_negotiated_ = true;
      protocol_negotiated_ = proxy_socket->GetProtocolNegotiated();
      SwitchToSpdyMode();
    }
  }

  // We may be using spdy without SSL
  if (ShouldForceSpdyWithoutSSL())
    SwitchToSpdyMode();

  if (result == ERR_PROXY_AUTH_REQUESTED ||
      result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    DCHECK(!ssl_started);
    // Other state (i.e. |using_ssl_|) suggests that |connection_| will have an
    // SSL socket, but there was an error before that could happen.  This
    // puts the in progress HttpProxy socket into |connection_| in order to
    // complete the auth (or read the response body).  The tunnel restart code
    // is careful to remove it before returning control to the rest of this
    // class.
    connection_.reset(connection_->release_pending_http_proxy_connection());
    return result;
  }

  if (!ssl_started && result < 0 && original_url_.get()) {
    job_status_ = STATUS_BROKEN;
    MaybeMarkAlternateProtocolBroken();
    return result;
  }

  if (using_quic_) {
    if (result < 0) {
      job_status_ = STATUS_BROKEN;
      MaybeMarkAlternateProtocolBroken();
      return result;
    }
    stream_ = quic_request_.ReleaseStream();
    next_state_ = STATE_NONE;
    return OK;
  }

  if (result < 0 && !ssl_started)
    return ReconsiderProxyAfterError(result);
  establishing_tunnel_ = false;

  if (connection_->socket()) {
    LogHttpConnectedMetrics(*connection_);

    // We officially have a new connection.  Record the type.
    if (!connection_->is_reused()) {
      ConnectionType type = using_spdy_ ? CONNECTION_SPDY : CONNECTION_HTTP;
      UpdateConnectionTypeHistograms(type);
    }
  }

  // Handle SSL errors below.
  if (using_ssl_) {
    DCHECK(ssl_started);
    if (IsCertificateError(result)) {
      if (using_spdy_ && original_url_.get() &&
          original_url_->SchemeIs("http")) {
        // We ignore certificate errors for http over spdy.
        spdy_certificate_error_ = result;
        result = OK;
      } else {
        result = HandleCertificateError(result);
        if (result == OK && !connection_->socket()->IsConnectedAndIdle()) {
          ReturnToStateInitConnection(true /* close connection */);
          return result;
        }
      }
    }
    if (result < 0)
      return result;
  }

  next_state_ = STATE_CREATE_STREAM;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoWaitingUserAction(int result) {
  // This state indicates that the stream request is in a partially
  // completed state, and we've called back to the delegate for more
  // information.

  // We're always waiting here for the delegate to call us back.
  return ERR_IO_PENDING;
}

int HttpStreamFactoryImpl::Job::DoCreateStream() {
  DCHECK(connection_->socket() || existing_spdy_session_.get() || using_quic_);

  next_state_ = STATE_CREATE_STREAM_COMPLETE;

  // We only set the socket motivation if we're the first to use
  // this socket.  Is there a race for two SPDY requests?  We really
  // need to plumb this through to the connect level.
  if (connection_->socket() && !connection_->is_reused())
    SetSocketMotivation();

  if (!using_spdy_) {
    // We may get ftp scheme when fetching ftp resources through proxy.
    bool using_proxy = (proxy_info_.is_http() || proxy_info_.is_https()) &&
                       (request_info_.url.SchemeIs("http") ||
                        request_info_.url.SchemeIs("ftp"));
    if (stream_factory_->for_websockets_) {
      DCHECK(request_);
      DCHECK(request_->websocket_handshake_stream_create_helper());
      websocket_stream_.reset(
          request_->websocket_handshake_stream_create_helper()
              ->CreateBasicStream(connection_.Pass(), using_proxy));
    } else {
      stream_.reset(new HttpBasicStream(connection_.release(), using_proxy));
    }
    return OK;
  }

  CHECK(!stream_.get());

  bool direct = true;
  const ProxyServer& proxy_server = proxy_info_.proxy_server();
  PrivacyMode privacy_mode = request_info_.privacy_mode;
  SpdySessionKey spdy_session_key(origin_, proxy_server, privacy_mode);
  if (IsHttpsProxyAndHttpUrl()) {
    // If we don't have a direct SPDY session, and we're using an HTTPS
    // proxy, then we might have a SPDY session to the proxy.
    // We never use privacy mode for connection to proxy server.
    spdy_session_key = SpdySessionKey(proxy_server.host_port_pair(),
                                      ProxyServer::Direct(),
                                      PRIVACY_MODE_DISABLED);
    direct = false;
  }

  base::WeakPtr<SpdySession> spdy_session;
  if (existing_spdy_session_.get()) {
    // We picked up an existing session, so we don't need our socket.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
    std::swap(spdy_session, existing_spdy_session_);
  } else {
    SpdySessionPool* spdy_pool = session_->spdy_session_pool();
    spdy_session = spdy_pool->FindAvailableSession(spdy_session_key, net_log_);
    if (!spdy_session) {
      base::WeakPtr<SpdySession> new_spdy_session =
          spdy_pool->CreateAvailableSessionFromSocket(spdy_session_key,
                                                      connection_.Pass(),
                                                      net_log_,
                                                      spdy_certificate_error_,
                                                      using_ssl_);
      if (!new_spdy_session->HasAcceptableTransportSecurity()) {
        new_spdy_session->CloseSessionOnError(
            ERR_SPDY_INADEQUATE_TRANSPORT_SECURITY, "");
        return ERR_SPDY_INADEQUATE_TRANSPORT_SECURITY;
      }

      new_spdy_session_ = new_spdy_session;
      spdy_session_direct_ = direct;
      const HostPortPair& host_port_pair = spdy_session_key.host_port_pair();
      base::WeakPtr<HttpServerProperties> http_server_properties =
          session_->http_server_properties();
      if (http_server_properties)
        http_server_properties->SetSupportsSpdy(host_port_pair, true);

      // Create a SpdyHttpStream attached to the session;
      // OnNewSpdySessionReadyCallback is not called until an event loop
      // iteration later, so if the SpdySession is closed between then, allow
      // reuse state from the underlying socket, sampled by SpdyHttpStream,
      // bubble up to the request.
      bool use_relative_url = direct || request_info_.url.SchemeIs("https");
      stream_.reset(new SpdyHttpStream(new_spdy_session_, use_relative_url));

      return OK;
    }
  }

  if (!spdy_session)
    return ERR_CONNECTION_CLOSED;

  // TODO(willchan): Delete this code, because eventually, the
  // HttpStreamFactoryImpl will be creating all the SpdyHttpStreams, since it
  // will know when SpdySessions become available.

  // TODO(ricea): Restore the code for WebSockets over SPDY once it's
  // implemented.
  if (stream_factory_->for_websockets_)
    return ERR_NOT_IMPLEMENTED;

  bool use_relative_url = direct || request_info_.url.SchemeIs("https");
  stream_.reset(new SpdyHttpStream(spdy_session, use_relative_url));

  return OK;
}

int HttpStreamFactoryImpl::Job::DoCreateStreamComplete(int result) {
  if (result < 0)
    return result;

  session_->proxy_service()->ReportSuccess(proxy_info_);
  next_state_ = STATE_NONE;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoRestartTunnelAuth() {
  next_state_ = STATE_RESTART_TUNNEL_AUTH_COMPLETE;
  ProxyClientSocket* proxy_socket =
      static_cast<ProxyClientSocket*>(connection_->socket());
  return proxy_socket->RestartWithAuth(io_callback_);
}

int HttpStreamFactoryImpl::Job::DoRestartTunnelAuthComplete(int result) {
  if (result == ERR_PROXY_AUTH_REQUESTED)
    return result;

  if (result == OK) {
    // Now that we've got the HttpProxyClientSocket connected.  We have
    // to release it as an idle socket into the pool and start the connection
    // process from the beginning.  Trying to pass it in with the
    // SSLSocketParams might cause a deadlock since params are dispatched
    // interchangeably.  This request won't necessarily get this http proxy
    // socket, but there will be forward progress.
    establishing_tunnel_ = false;
    ReturnToStateInitConnection(false /* do not close connection */);
    return OK;
  }

  return ReconsiderProxyAfterError(result);
}

void HttpStreamFactoryImpl::Job::ReturnToStateInitConnection(
    bool close_connection) {
  if (close_connection && connection_->socket())
    connection_->socket()->Disconnect();
  connection_->Reset();

  if (request_)
    request_->RemoveRequestFromSpdySessionRequestMap();

  next_state_ = STATE_INIT_CONNECTION;
}

void HttpStreamFactoryImpl::Job::SetSocketMotivation() {
  if (request_info_.motivation == HttpRequestInfo::PRECONNECT_MOTIVATED)
    connection_->socket()->SetSubresourceSpeculation();
  else if (request_info_.motivation == HttpRequestInfo::OMNIBOX_MOTIVATED)
    connection_->socket()->SetOmniboxSpeculation();
  // TODO(mbelshe): Add other motivations (like EARLY_LOAD_MOTIVATED).
}

bool HttpStreamFactoryImpl::Job::IsHttpsProxyAndHttpUrl() const {
  if (!proxy_info_.is_https())
    return false;
  if (original_url_.get()) {
    // We currently only support Alternate-Protocol where the original scheme
    // is http.
    DCHECK(original_url_->SchemeIs("http"));
    return original_url_->SchemeIs("http");
  }
  return request_info_.url.SchemeIs("http");
}

// Sets several fields of ssl_config for the given origin_server based on the
// proxy info and other factors.
void HttpStreamFactoryImpl::Job::InitSSLConfig(
    const HostPortPair& origin_server,
    SSLConfig* ssl_config,
    bool is_proxy) const {
  if (proxy_info_.is_https() && ssl_config->send_client_cert) {
    // When connecting through an HTTPS proxy, disable TLS False Start so
    // that client authentication errors can be distinguished between those
    // originating from the proxy server (ERR_PROXY_CONNECTION_FAILED) and
    // those originating from the endpoint (ERR_SSL_PROTOCOL_ERROR /
    // ERR_BAD_SSL_CLIENT_AUTH_CERT).
    // TODO(rch): This assumes that the HTTPS proxy will only request a
    // client certificate during the initial handshake.
    // http://crbug.com/59292
    ssl_config->false_start_enabled = false;
  }

  enum {
    FALLBACK_NONE = 0,    // SSL version fallback did not occur.
    FALLBACK_SSL3 = 1,    // Fell back to SSL 3.0.
    FALLBACK_TLS1 = 2,    // Fell back to TLS 1.0.
    FALLBACK_TLS1_1 = 3,  // Fell back to TLS 1.1.
    FALLBACK_MAX
  };

  int fallback = FALLBACK_NONE;
  if (ssl_config->version_fallback) {
    switch (ssl_config->version_max) {
      case SSL_PROTOCOL_VERSION_SSL3:
        fallback = FALLBACK_SSL3;
        break;
      case SSL_PROTOCOL_VERSION_TLS1:
        fallback = FALLBACK_TLS1;
        break;
      case SSL_PROTOCOL_VERSION_TLS1_1:
        fallback = FALLBACK_TLS1_1;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.ConnectionUsedSSLVersionFallback",
                            fallback, FALLBACK_MAX);

  // We also wish to measure the amount of fallback connections for a host that
  // we know implements TLS up to 1.2. Ideally there would be no fallback here
  // but high numbers of SSLv3 would suggest that SSLv3 fallback is being
  // caused by network middleware rather than buggy HTTPS servers.
  const std::string& host = origin_server.host();
  if (!is_proxy &&
      host.size() >= 10 &&
      host.compare(host.size() - 10, 10, "google.com") == 0 &&
      (host.size() == 10 || host[host.size()-11] == '.')) {
    UMA_HISTOGRAM_ENUMERATION("Net.GoogleConnectionUsedSSLVersionFallback",
                              fallback, FALLBACK_MAX);
  }

  if (request_info_.load_flags & LOAD_VERIFY_EV_CERT)
    ssl_config->verify_ev_cert = true;

  // Disable Channel ID if privacy mode is enabled.
  if (request_info_.privacy_mode == PRIVACY_MODE_ENABLED)
    ssl_config->channel_id_enabled = false;
}


int HttpStreamFactoryImpl::Job::ReconsiderProxyAfterError(int error) {
  DCHECK(!pac_request_);

  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.
  //
  switch (error) {
    case ERR_PROXY_CONNECTION_FAILED:
    case ERR_NAME_NOT_RESOLVED:
    case ERR_INTERNET_DISCONNECTED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_TIMED_OUT:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_CONNECTION_ABORTED:
    case ERR_TIMED_OUT:
    case ERR_TUNNEL_CONNECTION_FAILED:
    case ERR_SOCKS_CONNECTION_FAILED:
    // This can happen in the case of trying to talk to a proxy using SSL, and
    // ending up talking to a captive portal that supports SSL instead.
    case ERR_PROXY_CERTIFICATE_INVALID:
    // This can happen when trying to talk SSL to a non-SSL server (Like a
    // captive portal).
    case ERR_SSL_PROTOCOL_ERROR:
      break;
    case ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      // Remap the SOCKS-specific "host unreachable" error to a more
      // generic error code (this way consumers like the link doctor
      // know to substitute their error page).
      //
      // Note that if the host resolving was done by the SOCKS5 proxy, we can't
      // differentiate between a proxy-side "host not found" versus a proxy-side
      // "address unreachable" error, and will report both of these failures as
      // ERR_ADDRESS_UNREACHABLE.
      return ERR_ADDRESS_UNREACHABLE;
    default:
      return error;
  }

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    return error;
  }

  if (proxy_info_.is_https() && proxy_ssl_config_.send_client_cert) {
    session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair());
  }

  int rv = session_->proxy_service()->ReconsiderProxyAfterError(
      request_info_.url, error, &proxy_info_, io_callback_, &pac_request_,
      net_log_);
  if (rv == OK || rv == ERR_IO_PENDING) {
    // If the error was during connection setup, there is no socket to
    // disconnect.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
    if (request_)
      request_->RemoveRequestFromSpdySessionRequestMap();
    next_state_ = STATE_RESOLVE_PROXY_COMPLETE;
  } else {
    // If ReconsiderProxyAfterError() failed synchronously, it means
    // there was nothing left to fall-back to, so fail the transaction
    // with the last connection error we got.
    // TODO(eroman): This is a confusing contract, make it more obvious.
    rv = error;
  }

  return rv;
}

int HttpStreamFactoryImpl::Job::HandleCertificateError(int error) {
  DCHECK(using_ssl_);
  DCHECK(IsCertificateError(error));

  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  ssl_socket->GetSSLInfo(&ssl_info_);

  // Add the bad certificate to the set of allowed certificates in the
  // SSL config object. This data structure will be consulted after calling
  // RestartIgnoringLastError(). And the user will be asked interactively
  // before RestartIgnoringLastError() is ever called.
  SSLConfig::CertAndStatus bad_cert;

  // |ssl_info_.cert| may be NULL if we failed to create
  // X509Certificate for whatever reason, but normally it shouldn't
  // happen, unless this code is used inside sandbox.
  if (ssl_info_.cert.get() == NULL ||
      !X509Certificate::GetDEREncoded(ssl_info_.cert->os_cert_handle(),
                                      &bad_cert.der_cert)) {
    return error;
  }
  bad_cert.cert_status = ssl_info_.cert_status;
  server_ssl_config_.allowed_bad_certs.push_back(bad_cert);

  int load_flags = request_info_.load_flags;
  if (session_->params().ignore_certificate_errors)
    load_flags |= LOAD_IGNORE_ALL_CERT_ERRORS;
  if (ssl_socket->IgnoreCertError(error, load_flags))
    return OK;
  return error;
}

void HttpStreamFactoryImpl::Job::SwitchToSpdyMode() {
  if (HttpStreamFactory::spdy_enabled())
    using_spdy_ = true;
}

// static
void HttpStreamFactoryImpl::Job::LogHttpConnectedMetrics(
    const ClientSocketHandle& handle) {
  UMA_HISTOGRAM_ENUMERATION("Net.HttpSocketType", handle.reuse_type(),
                            ClientSocketHandle::NUM_TYPES);

  switch (handle.reuse_type()) {
    case ClientSocketHandle::UNUSED:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.HttpConnectionLatency",
                                 handle.setup_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;
    case ClientSocketHandle::UNUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SocketIdleTimeBeforeNextUse_UnusedSocket",
                                 handle.idle_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(6),
                                 100);
      break;
    case ClientSocketHandle::REUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SocketIdleTimeBeforeNextUse_ReusedSocket",
                                 handle.idle_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(6),
                                 100);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool HttpStreamFactoryImpl::Job::IsPreconnecting() const {
  DCHECK_GE(num_streams_, 0);
  return num_streams_ > 0;
}

bool HttpStreamFactoryImpl::Job::IsOrphaned() const {
  return !IsPreconnecting() && !request_;
}

void HttpStreamFactoryImpl::Job::ReportJobSuccededForRequest() {
  net::AlternateProtocolExperiment alternate_protocol_experiment =
      ALTERNATE_PROTOCOL_NOT_PART_OF_EXPERIMENT;
  base::WeakPtr<HttpServerProperties> http_server_properties =
      session_->http_server_properties();
  if (http_server_properties) {
    alternate_protocol_experiment =
        http_server_properties->GetAlternateProtocolExperiment();
  }
  if (using_existing_quic_session_) {
    // If an existing session was used, then no TCP connection was
    // started.
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_NO_RACE,
                                    alternate_protocol_experiment);
  } else if (original_url_) {
    // This job was the alternate protocol job, and hence won the race.
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_WON_RACE,
                                    alternate_protocol_experiment);
  } else {
    // This job was the normal job, and hence the alternate protocol job lost
    // the race.
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_LOST_RACE,
                                    alternate_protocol_experiment);
  }
}

void HttpStreamFactoryImpl::Job::MarkOtherJobComplete(const Job& job) {
  DCHECK_EQ(STATUS_RUNNING, other_job_status_);
  other_job_status_ = job.job_status_;
  MaybeMarkAlternateProtocolBroken();
}

void HttpStreamFactoryImpl::Job::MaybeMarkAlternateProtocolBroken() {
  if (job_status_ == STATUS_RUNNING || other_job_status_ == STATUS_RUNNING)
    return;

  bool is_alternate_protocol_job = original_url_.get() != NULL;
  if (is_alternate_protocol_job) {
    if (job_status_ == STATUS_BROKEN && other_job_status_ == STATUS_SUCCEEDED) {
      HistogramBrokenAlternateProtocolLocation(
          BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB_ALT);
      session_->http_server_properties()->SetBrokenAlternateProtocol(
          HostPortPair::FromURL(*original_url_));
    }
    return;
  }

  if (job_status_ == STATUS_SUCCEEDED && other_job_status_ == STATUS_BROKEN) {
    HistogramBrokenAlternateProtocolLocation(
        BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB_MAIN);
    session_->http_server_properties()->SetBrokenAlternateProtocol(
        HostPortPair::FromURL(request_info_.url));
  }
}

}  // namespace net
