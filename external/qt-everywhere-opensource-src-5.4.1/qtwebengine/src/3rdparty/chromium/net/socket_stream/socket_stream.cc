// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(ukai): code is similar with http_network_transaction.cc.  We should
//   think about ways to share code, if possible.

#include "net/socket_stream/socket_stream.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks5_client_socket.h"
#include "net/socket/socks_client_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket_stream/socket_stream_metrics.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

static const int kMaxPendingSendAllowed = 32768;  // 32 kilobytes.
static const int kReadBufferSize = 4096;

namespace net {

int SocketStream::Delegate::OnStartOpenConnection(
    SocketStream* socket, const CompletionCallback& callback) {
  return OK;
}

void SocketStream::Delegate::OnAuthRequired(SocketStream* socket,
                                            AuthChallengeInfo* auth_info) {
  // By default, no credential is available and close the connection.
  socket->Close();
}

void SocketStream::Delegate::OnSSLCertificateError(
    SocketStream* socket,
    const SSLInfo& ssl_info,
    bool fatal) {
  socket->CancelWithSSLError(ssl_info);
}

bool SocketStream::Delegate::CanGetCookies(SocketStream* socket,
                                           const GURL& url) {
  return true;
}

bool SocketStream::Delegate::CanSetCookie(SocketStream* request,
                                          const GURL& url,
                                          const std::string& cookie_line,
                                          CookieOptions* options) {
  return true;
}

SocketStream::ResponseHeaders::ResponseHeaders() : IOBuffer() {}

void SocketStream::ResponseHeaders::Realloc(size_t new_size) {
  headers_.reset(static_cast<char*>(realloc(headers_.release(), new_size)));
}

SocketStream::ResponseHeaders::~ResponseHeaders() { data_ = NULL; }

SocketStream::SocketStream(const GURL& url, Delegate* delegate,
                           URLRequestContext* context,
                           CookieStore* cookie_store)
    : delegate_(delegate),
      url_(url),
      max_pending_send_allowed_(kMaxPendingSendAllowed),
      context_(context),
      next_state_(STATE_NONE),
      factory_(ClientSocketFactory::GetDefaultFactory()),
      proxy_mode_(kDirectConnection),
      proxy_url_(url),
      pac_request_(NULL),
      connection_(new ClientSocketHandle),
      privacy_mode_(PRIVACY_MODE_DISABLED),
      // Unretained() is required; without it, Bind() creates a circular
      // dependency and the SocketStream object will not be freed.
      io_callback_(base::Bind(&SocketStream::OnIOCompleted,
                              base::Unretained(this))),
      read_buf_(NULL),
      current_write_buf_(NULL),
      waiting_for_write_completion_(false),
      closing_(false),
      server_closed_(false),
      metrics_(new SocketStreamMetrics(url)),
      cookie_store_(cookie_store) {
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";
  DCHECK(base::MessageLoopForIO::IsCurrent())
      << "The current base::MessageLoop must be TYPE_IO";
  DCHECK(delegate_);

  if (context_) {
    if (!cookie_store_)
      cookie_store_ = context_->cookie_store();

    net_log_ = BoundNetLog::Make(
        context->net_log(),
        NetLog::SOURCE_SOCKET_STREAM);

    net_log_.BeginEvent(NetLog::TYPE_REQUEST_ALIVE);
  }
}

SocketStream::UserData* SocketStream::GetUserData(
    const void* key) const {
  UserDataMap::const_iterator found = user_data_.find(key);
  if (found != user_data_.end())
    return found->second.get();
  return NULL;
}

void SocketStream::SetUserData(const void* key, UserData* data) {
  user_data_[key] = linked_ptr<UserData>(data);
}

bool SocketStream::is_secure() const {
  return url_.SchemeIs("wss");
}

void SocketStream::DetachContext() {
  if (!context_)
    return;

  if (pac_request_) {
    context_->proxy_service()->CancelPacRequest(pac_request_);
    pac_request_ = NULL;
  }

  net_log_.EndEvent(NetLog::TYPE_REQUEST_ALIVE);
  net_log_ = BoundNetLog();

  context_ = NULL;
  cookie_store_ = NULL;
}

void SocketStream::CheckPrivacyMode() {
  if (context_ && context_->network_delegate()) {
    bool enable = context_->network_delegate()->CanEnablePrivacyMode(url_,
                                                                     url_);
    privacy_mode_ = enable ? PRIVACY_MODE_ENABLED : PRIVACY_MODE_DISABLED;
    // Disable Channel ID if privacy mode is enabled.
    if (enable)
      server_ssl_config_.channel_id_enabled = false;
  }
}

void SocketStream::Connect() {
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";
  DCHECK(base::MessageLoopForIO::IsCurrent())
      << "The current base::MessageLoop must be TYPE_IO";
  if (context_) {
    context_->ssl_config_service()->GetSSLConfig(&server_ssl_config_);
    proxy_ssl_config_ = server_ssl_config_;
  }
  CheckPrivacyMode();

  DCHECK_EQ(next_state_, STATE_NONE);

  AddRef();  // Released in Finish()
  // Open a connection asynchronously, so that delegate won't be called
  // back before returning Connect().
  next_state_ = STATE_BEFORE_CONNECT;
  net_log_.BeginEvent(
      NetLog::TYPE_SOCKET_STREAM_CONNECT,
      NetLog::StringCallback("url", &url_.possibly_invalid_spec()));
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SocketStream::DoLoop, this, OK));
}

size_t SocketStream::GetTotalSizeOfPendingWriteBufs() const {
  size_t total_size = 0;
  for (PendingDataQueue::const_iterator iter = pending_write_bufs_.begin();
       iter != pending_write_bufs_.end();
       ++iter)
    total_size += (*iter)->size();
  return total_size;
}

bool SocketStream::SendData(const char* data, int len) {
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";
  DCHECK(base::MessageLoopForIO::IsCurrent())
      << "The current base::MessageLoop must be TYPE_IO";
  DCHECK_GT(len, 0);

  if (!connection_->socket() ||
      !connection_->socket()->IsConnected() || next_state_ == STATE_NONE) {
    return false;
  }

  int total_buffered_bytes = len;
  if (current_write_buf_.get()) {
    // Since
    // - the purpose of this check is to limit the amount of buffer used by
    //   this instance.
    // - the DrainableIOBuffer doesn't release consumed memory.
    // we need to use not BytesRemaining() but size() here.
    total_buffered_bytes += current_write_buf_->size();
  }
  total_buffered_bytes += GetTotalSizeOfPendingWriteBufs();
  if (total_buffered_bytes > max_pending_send_allowed_)
    return false;

  // TODO(tyoshino): Split data into smaller chunks e.g. 8KiB to free consumed
  // buffer progressively
  pending_write_bufs_.push_back(make_scoped_refptr(
      new IOBufferWithSize(len)));
  memcpy(pending_write_bufs_.back()->data(), data, len);

  // If current_write_buf_ is not NULL, it means that a) there's ongoing write
  // operation or b) the connection is being closed. If a), the buffer we just
  // pushed will be automatically handled when the completion callback runs
  // the loop, and therefore we don't need to enqueue DoLoop(). If b), it's ok
  // to do nothing. If current_write_buf_ is NULL, to make sure DoLoop() is
  // ran soon, enequeue it.
  if (!current_write_buf_.get()) {
    // Send pending data asynchronously, so that delegate won't be called
    // back before returning from SendData().
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&SocketStream::DoLoop, this, OK));
  }

  return true;
}

void SocketStream::Close() {
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";
  DCHECK(base::MessageLoopForIO::IsCurrent())
      << "The current base::MessageLoop must be TYPE_IO";
  // If next_state_ is STATE_NONE, the socket was not opened, or already
  // closed.  So, return immediately.
  // Otherwise, it might call Finish() more than once, so breaks balance
  // of AddRef() and Release() in Connect() and Finish(), respectively.
  if (next_state_ == STATE_NONE)
    return;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SocketStream::DoClose, this));
}

void SocketStream::RestartWithAuth(const AuthCredentials& credentials) {
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";
  DCHECK(base::MessageLoopForIO::IsCurrent())
      << "The current base::MessageLoop must be TYPE_IO";
  DCHECK(proxy_auth_controller_.get());
  if (!connection_->socket()) {
    DVLOG(1) << "Socket is closed before restarting with auth.";
    return;
  }

  proxy_auth_controller_->ResetAuth(credentials);

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SocketStream::DoRestartWithAuth, this));
}

void SocketStream::DetachDelegate() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  // Prevent the rest of the function from executing if we are being called from
  // within Finish().
  if (next_state_ == STATE_NONE)
    return;
  net_log_.AddEvent(NetLog::TYPE_CANCELLED);
  // We don't need to send pending data when client detach the delegate.
  pending_write_bufs_.clear();
  Close();
}

const ProxyServer& SocketStream::proxy_server() const {
  return proxy_info_.proxy_server();
}

void SocketStream::SetClientSocketFactory(
    ClientSocketFactory* factory) {
  DCHECK(factory);
  factory_ = factory;
}

void SocketStream::CancelWithError(int error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SocketStream::DoLoop, this, error));
}

void SocketStream::CancelWithSSLError(const SSLInfo& ssl_info) {
  CancelWithError(MapCertStatusToNetError(ssl_info.cert_status));
}

void SocketStream::ContinueDespiteError() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SocketStream::DoLoop, this, OK));
}

SocketStream::~SocketStream() {
  DetachContext();
  DCHECK(!delegate_);
  DCHECK(!pac_request_);
}

SocketStream::RequestHeaders::~RequestHeaders() { data_ = NULL; }

void SocketStream::set_addresses(const AddressList& addresses) {
  addresses_ = addresses;
}

void SocketStream::DoClose() {
  closing_ = true;
  // If next_state_ is:
  // - STATE_TCP_CONNECT_COMPLETE, it's waiting other socket establishing
  //   connection.
  // - STATE_AUTH_REQUIRED, it's waiting for restarting.
  // - STATE_RESOLVE_PROTOCOL_COMPLETE, it's waiting for delegate_ to finish
  //   OnStartOpenConnection method call
  // In these states, we'll close the SocketStream now.
  if (next_state_ == STATE_TCP_CONNECT_COMPLETE ||
      next_state_ == STATE_AUTH_REQUIRED ||
      next_state_ == STATE_RESOLVE_PROTOCOL_COMPLETE) {
    DoLoop(ERR_ABORTED);
    return;
  }
  // If next_state_ is STATE_READ_WRITE, we'll run DoLoop and close
  // the SocketStream.
  // If it's writing now, we should defer the closing after the current
  // writing is completed.
  if (next_state_ == STATE_READ_WRITE && !current_write_buf_.get())
    DoLoop(ERR_ABORTED);

  // In other next_state_, we'll wait for callback of other APIs, such as
  // ResolveProxy().
}

void SocketStream::Finish(int result) {
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";
  DCHECK(base::MessageLoopForIO::IsCurrent())
      << "The current base::MessageLoop must be TYPE_IO";
  DCHECK_LE(result, OK);
  if (result == OK)
    result = ERR_CONNECTION_CLOSED;
  DCHECK_EQ(next_state_, STATE_NONE);
  DVLOG(1) << "Finish result=" << ErrorToString(result);

  metrics_->OnClose();

  if (result != ERR_CONNECTION_CLOSED && delegate_)
    delegate_->OnError(this, result);
  if (result != ERR_PROTOCOL_SWITCHED && delegate_)
    delegate_->OnClose(this);
  delegate_ = NULL;

  Release();
}

int SocketStream::DidEstablishConnection() {
  if (!connection_->socket() || !connection_->socket()->IsConnected()) {
    next_state_ = STATE_CLOSE;
    return ERR_CONNECTION_FAILED;
  }
  next_state_ = STATE_READ_WRITE;
  metrics_->OnConnected();

  net_log_.EndEvent(NetLog::TYPE_SOCKET_STREAM_CONNECT);
  if (delegate_)
    delegate_->OnConnected(this, max_pending_send_allowed_);

  return OK;
}

int SocketStream::DidReceiveData(int result) {
  DCHECK(read_buf_.get());
  DCHECK_GT(result, 0);
  net_log_.AddEvent(NetLog::TYPE_SOCKET_STREAM_RECEIVED);
  int len = result;
  metrics_->OnRead(len);
  if (delegate_) {
    // Notify recevied data to delegate.
    delegate_->OnReceivedData(this, read_buf_->data(), len);
  }
  read_buf_ = NULL;
  return OK;
}

void SocketStream::DidSendData(int result) {
  DCHECK_GT(result, 0);
  DCHECK(current_write_buf_.get());
  net_log_.AddEvent(NetLog::TYPE_SOCKET_STREAM_SENT);

  int bytes_sent = result;

  metrics_->OnWrite(bytes_sent);

  current_write_buf_->DidConsume(result);

  if (current_write_buf_->BytesRemaining())
    return;

  size_t bytes_freed = current_write_buf_->size();

  current_write_buf_ = NULL;

  // We freed current_write_buf_ and this instance is now able to accept more
  // data via SendData() (note that DidConsume() doesn't free consumed memory).
  // We can tell that to delegate_ by calling OnSentData().
  if (delegate_)
    delegate_->OnSentData(this, bytes_freed);
}

void SocketStream::OnIOCompleted(int result) {
  DoLoop(result);
}

void SocketStream::OnReadCompleted(int result) {
  if (result == 0) {
    // 0 indicates end-of-file, so socket was closed.
    // Don't close the socket if it's still writing.
    server_closed_ = true;
  } else if (result > 0 && read_buf_.get()) {
    result = DidReceiveData(result);
  }
  DoLoop(result);
}

void SocketStream::OnWriteCompleted(int result) {
  waiting_for_write_completion_ = false;
  if (result > 0) {
    DidSendData(result);
    result = OK;
  }
  DoLoop(result);
}

void SocketStream::DoLoop(int result) {
  if (next_state_ == STATE_NONE)
    return;

  // If context was not set, close immediately.
  if (!context_)
    next_state_ = STATE_CLOSE;

  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_BEFORE_CONNECT:
        DCHECK_EQ(OK, result);
        result = DoBeforeConnect();
        break;
      case STATE_BEFORE_CONNECT_COMPLETE:
        result = DoBeforeConnectComplete(result);
        break;
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, result);
        result = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        result = DoResolveProxyComplete(result);
        break;
      case STATE_RESOLVE_HOST:
        DCHECK_EQ(OK, result);
        result = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        result = DoResolveHostComplete(result);
        break;
      case STATE_RESOLVE_PROTOCOL:
        result = DoResolveProtocol(result);
        break;
      case STATE_RESOLVE_PROTOCOL_COMPLETE:
        result = DoResolveProtocolComplete(result);
        break;
      case STATE_TCP_CONNECT:
        result = DoTcpConnect(result);
        break;
      case STATE_TCP_CONNECT_COMPLETE:
        result = DoTcpConnectComplete(result);
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN:
        result = DoGenerateProxyAuthToken();
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
        result = DoGenerateProxyAuthTokenComplete(result);
        break;
      case STATE_WRITE_TUNNEL_HEADERS:
        DCHECK_EQ(OK, result);
        result = DoWriteTunnelHeaders();
        break;
      case STATE_WRITE_TUNNEL_HEADERS_COMPLETE:
        result = DoWriteTunnelHeadersComplete(result);
        break;
      case STATE_READ_TUNNEL_HEADERS:
        DCHECK_EQ(OK, result);
        result = DoReadTunnelHeaders();
        break;
      case STATE_READ_TUNNEL_HEADERS_COMPLETE:
        result = DoReadTunnelHeadersComplete(result);
        break;
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, result);
        result = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        result = DoSOCKSConnectComplete(result);
        break;
      case STATE_SECURE_PROXY_CONNECT:
        DCHECK_EQ(OK, result);
        result = DoSecureProxyConnect();
        break;
      case STATE_SECURE_PROXY_CONNECT_COMPLETE:
        result = DoSecureProxyConnectComplete(result);
        break;
      case STATE_SECURE_PROXY_HANDLE_CERT_ERROR:
        result = DoSecureProxyHandleCertError(result);
        break;
      case STATE_SECURE_PROXY_HANDLE_CERT_ERROR_COMPLETE:
        result = DoSecureProxyHandleCertErrorComplete(result);
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, result);
        result = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        result = DoSSLConnectComplete(result);
        break;
      case STATE_SSL_HANDLE_CERT_ERROR:
        result = DoSSLHandleCertError(result);
        break;
      case STATE_SSL_HANDLE_CERT_ERROR_COMPLETE:
        result = DoSSLHandleCertErrorComplete(result);
        break;
      case STATE_READ_WRITE:
        result = DoReadWrite(result);
        break;
      case STATE_AUTH_REQUIRED:
        // It might be called when DoClose is called while waiting in
        // STATE_AUTH_REQUIRED.
        Finish(result);
        return;
      case STATE_CLOSE:
        DCHECK_LE(result, OK);
        Finish(result);
        return;
      default:
        NOTREACHED() << "bad state " << state;
        Finish(result);
        return;
    }
    if (state == STATE_RESOLVE_PROTOCOL && result == ERR_PROTOCOL_SWITCHED)
      continue;
    // If the connection is not established yet and had actual errors,
    // record the error.  In next iteration, it will close the connection.
    if (state != STATE_READ_WRITE && result < ERR_IO_PENDING) {
      net_log_.EndEventWithNetErrorCode(
          NetLog::TYPE_SOCKET_STREAM_CONNECT, result);
    }
  } while (result != ERR_IO_PENDING);
}

int SocketStream::DoBeforeConnect() {
  next_state_ = STATE_BEFORE_CONNECT_COMPLETE;
  if (!context_ || !context_->network_delegate())
    return OK;

  int result = context_->network_delegate()->NotifyBeforeSocketStreamConnect(
      this, io_callback_);
  if (result != OK && result != ERR_IO_PENDING)
    next_state_ = STATE_CLOSE;

  return result;
}

int SocketStream::DoBeforeConnectComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result == OK)
    next_state_ = STATE_RESOLVE_PROXY;
  else
    next_state_ = STATE_CLOSE;

  return result;
}

int SocketStream::DoResolveProxy() {
  DCHECK(context_);
  DCHECK(!pac_request_);
  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (!proxy_url_.is_valid()) {
    next_state_ = STATE_CLOSE;
    return ERR_INVALID_ARGUMENT;
  }

  // TODO(toyoshim): Check server advertisement of SPDY through the HTTP
  // Alternate-Protocol header, then switch to SPDY if SPDY is available.
  // Usually we already have a session to the SPDY server because JavaScript
  // running WebSocket itself would be served by SPDY. But, in some situation
  // (E.g. Used by Chrome Extensions or used for cross origin connection), this
  // connection might be the first one. At that time, we should check
  // Alternate-Protocol header here for ws:// or TLS NPN extension for wss:// .

  return context_->proxy_service()->ResolveProxy(
      proxy_url_, &proxy_info_, io_callback_, &pac_request_, net_log_);
}

int SocketStream::DoResolveProxyComplete(int result) {
  pac_request_ = NULL;
  if (result != OK) {
    DVLOG(1) << "Failed to resolve proxy: " << result;
    if (delegate_)
      delegate_->OnError(this, result);
    proxy_info_.UseDirect();
  }
  if (proxy_info_.is_direct()) {
    // If proxy was not found for original URL (i.e. websocket URL),
    // try again with https URL, like Safari implementation.
    // Note that we don't want to use http proxy, because we'll use tunnel
    // proxy using CONNECT method, which is used by https proxy.
    if (!proxy_url_.SchemeIs("https")) {
      const std::string scheme = "https";
      GURL::Replacements repl;
      repl.SetSchemeStr(scheme);
      proxy_url_ = url_.ReplaceComponents(repl);
      DVLOG(1) << "Try https proxy: " << proxy_url_;
      next_state_ = STATE_RESOLVE_PROXY;
      return OK;
    }
  }

  if (proxy_info_.is_empty()) {
    // No proxies/direct to choose from. This happens when we don't support any
    // of the proxies in the returned list.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  next_state_ = STATE_RESOLVE_HOST;
  return OK;
}

int SocketStream::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;

  DCHECK(!proxy_info_.is_empty());
  if (proxy_info_.is_direct())
    proxy_mode_ = kDirectConnection;
  else if (proxy_info_.proxy_server().is_socks())
    proxy_mode_ = kSOCKSProxy;
  else
    proxy_mode_ = kTunnelProxy;

  // Determine the host and port to connect to.
  HostPortPair host_port_pair;
  if (proxy_mode_ != kDirectConnection) {
    host_port_pair = proxy_info_.proxy_server().host_port_pair();
  } else {
    host_port_pair = HostPortPair::FromURL(url_);
  }

  HostResolver::RequestInfo resolve_info(host_port_pair);

  DCHECK(context_->host_resolver());
  resolver_.reset(new SingleRequestHostResolver(context_->host_resolver()));
  return resolver_->Resolve(resolve_info,
                            DEFAULT_PRIORITY,
                            &addresses_,
                            base::Bind(&SocketStream::OnIOCompleted, this),
                            net_log_);
}

int SocketStream::DoResolveHostComplete(int result) {
  if (result == OK)
    next_state_ = STATE_RESOLVE_PROTOCOL;
  else
    next_state_ = STATE_CLOSE;
  // TODO(ukai): if error occured, reconsider proxy after error.
  return result;
}

int SocketStream::DoResolveProtocol(int result) {
  DCHECK_EQ(OK, result);

  if (!delegate_) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  next_state_ = STATE_RESOLVE_PROTOCOL_COMPLETE;
  result = delegate_->OnStartOpenConnection(this, io_callback_);
  if (result == ERR_IO_PENDING)
    metrics_->OnWaitConnection();
  else if (result != OK && result != ERR_PROTOCOL_SWITCHED)
    next_state_ = STATE_CLOSE;
  return result;
}

int SocketStream::DoResolveProtocolComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result == ERR_PROTOCOL_SWITCHED) {
    next_state_ = STATE_CLOSE;
    metrics_->OnCountWireProtocolType(
        SocketStreamMetrics::WIRE_PROTOCOL_SPDY);
  } else if (result == OK) {
    next_state_ = STATE_TCP_CONNECT;
    metrics_->OnCountWireProtocolType(
        SocketStreamMetrics::WIRE_PROTOCOL_WEBSOCKET);
  } else {
    next_state_ = STATE_CLOSE;
  }
  return result;
}

int SocketStream::DoTcpConnect(int result) {
  if (result != OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }
  next_state_ = STATE_TCP_CONNECT_COMPLETE;
  DCHECK(factory_);
  connection_->SetSocket(
      factory_->CreateTransportClientSocket(addresses_,
                                            net_log_.net_log(),
                                            net_log_.source()));
  metrics_->OnStartConnection();
  return connection_->socket()->Connect(io_callback_);
}

int SocketStream::DoTcpConnectComplete(int result) {
  // TODO(ukai): if error occured, reconsider proxy after error.
  if (result != OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  if (proxy_mode_ == kTunnelProxy) {
    if (proxy_info_.is_https())
      next_state_ = STATE_SECURE_PROXY_CONNECT;
    else
      next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  } else if (proxy_mode_ == kSOCKSProxy) {
    next_state_ = STATE_SOCKS_CONNECT;
  } else if (is_secure()) {
    next_state_ = STATE_SSL_CONNECT;
  } else {
    result = DidEstablishConnection();
  }
  return result;
}

int SocketStream::DoGenerateProxyAuthToken() {
  next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE;
  if (!proxy_auth_controller_.get()) {
    DCHECK(context_);
    DCHECK(context_->http_transaction_factory());
    DCHECK(context_->http_transaction_factory()->GetSession());
    HttpNetworkSession* session =
        context_->http_transaction_factory()->GetSession();
    const char* scheme = proxy_info_.is_https() ? "https://" : "http://";
    GURL auth_url(scheme +
                  proxy_info_.proxy_server().host_port_pair().ToString());
    proxy_auth_controller_ =
        new HttpAuthController(HttpAuth::AUTH_PROXY,
                               auth_url,
                               session->http_auth_cache(),
                               session->http_auth_handler_factory());
  }
  HttpRequestInfo request_info;
  request_info.url = url_;
  request_info.method = "CONNECT";
  return proxy_auth_controller_->MaybeGenerateAuthToken(
      &request_info, io_callback_, net_log_);
}

int SocketStream::DoGenerateProxyAuthTokenComplete(int result) {
  if (result != OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  next_state_ = STATE_WRITE_TUNNEL_HEADERS;
  return result;
}

int SocketStream::DoWriteTunnelHeaders() {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  next_state_ = STATE_WRITE_TUNNEL_HEADERS_COMPLETE;

  if (!tunnel_request_headers_.get()) {
    metrics_->OnCountConnectionType(SocketStreamMetrics::TUNNEL_CONNECTION);
    tunnel_request_headers_ = new RequestHeaders();
    tunnel_request_headers_bytes_sent_ = 0;
  }
  if (tunnel_request_headers_->headers_.empty()) {
    HttpRequestHeaders request_headers;
    request_headers.SetHeader("Host", GetHostAndOptionalPort(url_));
    request_headers.SetHeader("Proxy-Connection", "keep-alive");
    if (proxy_auth_controller_.get() && proxy_auth_controller_->HaveAuth())
      proxy_auth_controller_->AddAuthorizationHeader(&request_headers);
    tunnel_request_headers_->headers_ = base::StringPrintf(
        "CONNECT %s HTTP/1.1\r\n"
        "%s",
        GetHostAndPort(url_).c_str(),
        request_headers.ToString().c_str());
  }
  tunnel_request_headers_->SetDataOffset(tunnel_request_headers_bytes_sent_);
  int buf_len = static_cast<int>(tunnel_request_headers_->headers_.size() -
                                 tunnel_request_headers_bytes_sent_);
  DCHECK_GT(buf_len, 0);
  return connection_->socket()->Write(
      tunnel_request_headers_.get(), buf_len, io_callback_);
}

int SocketStream::DoWriteTunnelHeadersComplete(int result) {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  if (result < 0) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  tunnel_request_headers_bytes_sent_ += result;
  if (tunnel_request_headers_bytes_sent_ <
      tunnel_request_headers_->headers_.size()) {
    next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  } else {
    // Handling a cert error or a client cert request requires reconnection.
    // DoWriteTunnelHeaders() will be called again.
    // Thus |tunnel_request_headers_bytes_sent_| should be reset to 0 for
    // sending |tunnel_request_headers_| correctly.
    tunnel_request_headers_bytes_sent_ = 0;
    next_state_ = STATE_READ_TUNNEL_HEADERS;
  }
  return OK;
}

int SocketStream::DoReadTunnelHeaders() {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  next_state_ = STATE_READ_TUNNEL_HEADERS_COMPLETE;

  if (!tunnel_response_headers_.get()) {
    tunnel_response_headers_ = new ResponseHeaders();
    tunnel_response_headers_capacity_ = kMaxTunnelResponseHeadersSize;
    tunnel_response_headers_->Realloc(tunnel_response_headers_capacity_);
    tunnel_response_headers_len_ = 0;
  }

  int buf_len = tunnel_response_headers_capacity_ -
      tunnel_response_headers_len_;
  tunnel_response_headers_->SetDataOffset(tunnel_response_headers_len_);
  CHECK(tunnel_response_headers_->data());

  return connection_->socket()->Read(
      tunnel_response_headers_.get(), buf_len, io_callback_);
}

int SocketStream::DoReadTunnelHeadersComplete(int result) {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  if (result < 0) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  if (result == 0) {
    // 0 indicates end-of-file, so socket was closed.
    next_state_ = STATE_CLOSE;
    return ERR_CONNECTION_CLOSED;
  }

  tunnel_response_headers_len_ += result;
  DCHECK(tunnel_response_headers_len_ <= tunnel_response_headers_capacity_);

  int eoh = HttpUtil::LocateEndOfHeaders(
      tunnel_response_headers_->headers(), tunnel_response_headers_len_, 0);
  if (eoh == -1) {
    if (tunnel_response_headers_len_ >= kMaxTunnelResponseHeadersSize) {
      next_state_ = STATE_CLOSE;
      return ERR_RESPONSE_HEADERS_TOO_BIG;
    }

    next_state_ = STATE_READ_TUNNEL_HEADERS;
    return OK;
  }
  // DidReadResponseHeaders
  scoped_refptr<HttpResponseHeaders> headers;
  headers = new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(tunnel_response_headers_->headers(), eoh));
  if (headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // Require the "HTTP/1.x" status line.
    next_state_ = STATE_CLOSE;
    return ERR_TUNNEL_CONNECTION_FAILED;
  }
  switch (headers->response_code()) {
    case 200:  // OK
      if (is_secure()) {
        DCHECK_EQ(eoh, tunnel_response_headers_len_);
        next_state_ = STATE_SSL_CONNECT;
      } else {
        result = DidEstablishConnection();
        if (result < 0) {
          next_state_ = STATE_CLOSE;
          return result;
        }
        if ((eoh < tunnel_response_headers_len_) && delegate_)
          delegate_->OnReceivedData(
              this, tunnel_response_headers_->headers() + eoh,
              tunnel_response_headers_len_ - eoh);
      }
      return OK;
    case 407:  // Proxy Authentication Required.
      if (proxy_mode_ != kTunnelProxy)
        return ERR_UNEXPECTED_PROXY_AUTH;

      result = proxy_auth_controller_->HandleAuthChallenge(
          headers, false, true, net_log_);
      if (result != OK)
        return result;
      DCHECK(!proxy_info_.is_empty());
      next_state_ = STATE_AUTH_REQUIRED;
      if (proxy_auth_controller_->HaveAuth()) {
        base::MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(&SocketStream::DoRestartWithAuth, this));
        return ERR_IO_PENDING;
      }
      if (delegate_) {
        // Wait until RestartWithAuth or Close is called.
        base::MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(&SocketStream::DoAuthRequired, this));
        return ERR_IO_PENDING;
      }
      break;
    default:
      break;
  }
  next_state_ = STATE_CLOSE;
  return ERR_TUNNEL_CONNECTION_FAILED;
}

int SocketStream::DoSOCKSConnect() {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;

  HostResolver::RequestInfo req_info(HostPortPair::FromURL(url_));

  DCHECK(!proxy_info_.is_empty());
  scoped_ptr<StreamSocket> s;
  if (proxy_info_.proxy_server().scheme() == ProxyServer::SCHEME_SOCKS5) {
    s.reset(new SOCKS5ClientSocket(connection_.Pass(), req_info));
  } else {
    s.reset(new SOCKSClientSocket(connection_.Pass(),
                                  req_info,
                                  DEFAULT_PRIORITY,
                                  context_->host_resolver()));
  }
  connection_.reset(new ClientSocketHandle);
  connection_->SetSocket(s.Pass());
  metrics_->OnCountConnectionType(SocketStreamMetrics::SOCKS_CONNECTION);
  return connection_->socket()->Connect(io_callback_);
}

int SocketStream::DoSOCKSConnectComplete(int result) {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  if (result == OK) {
    if (is_secure())
      next_state_ = STATE_SSL_CONNECT;
    else
      result = DidEstablishConnection();
  } else {
    next_state_ = STATE_CLOSE;
  }
  return result;
}

int SocketStream::DoSecureProxyConnect() {
  DCHECK(factory_);
  SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = context_->cert_verifier();
  ssl_context.transport_security_state = context_->transport_security_state();
  ssl_context.server_bound_cert_service = context_->server_bound_cert_service();
  scoped_ptr<StreamSocket> socket(factory_->CreateSSLClientSocket(
      connection_.Pass(),
      proxy_info_.proxy_server().host_port_pair(),
      proxy_ssl_config_,
      ssl_context));
  connection_.reset(new ClientSocketHandle);
  connection_->SetSocket(socket.Pass());
  next_state_ = STATE_SECURE_PROXY_CONNECT_COMPLETE;
  metrics_->OnCountConnectionType(SocketStreamMetrics::SECURE_PROXY_CONNECTION);
  return connection_->socket()->Connect(io_callback_);
}

int SocketStream::DoSecureProxyConnectComplete(int result) {
  DCHECK_EQ(STATE_NONE, next_state_);
  // Reconnect with client authentication.
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED)
    return HandleCertificateRequest(result, &proxy_ssl_config_);

  if (IsCertificateError(result))
    next_state_ = STATE_SECURE_PROXY_HANDLE_CERT_ERROR;
  else if (result == OK)
    next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  else
    next_state_ = STATE_CLOSE;
  return result;
}

int SocketStream::DoSecureProxyHandleCertError(int result) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(IsCertificateError(result));
  result = HandleCertificateError(result);
  if (result == ERR_IO_PENDING)
    next_state_ = STATE_SECURE_PROXY_HANDLE_CERT_ERROR_COMPLETE;
  else
    next_state_ = STATE_CLOSE;
  return result;
}

int SocketStream::DoSecureProxyHandleCertErrorComplete(int result) {
  DCHECK_EQ(STATE_NONE, next_state_);
  if (result == OK) {
    if (!connection_->socket()->IsConnectedAndIdle())
      return AllowCertErrorForReconnection(&proxy_ssl_config_);
    next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  } else {
    next_state_ = STATE_CLOSE;
  }
  return result;
}

int SocketStream::DoSSLConnect() {
  DCHECK(factory_);
  SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = context_->cert_verifier();
  ssl_context.transport_security_state = context_->transport_security_state();
  ssl_context.server_bound_cert_service = context_->server_bound_cert_service();
  scoped_ptr<StreamSocket> socket(
      factory_->CreateSSLClientSocket(connection_.Pass(),
                                      HostPortPair::FromURL(url_),
                                      server_ssl_config_,
                                      ssl_context));
  connection_.reset(new ClientSocketHandle);
  connection_->SetSocket(socket.Pass());
  next_state_ = STATE_SSL_CONNECT_COMPLETE;
  metrics_->OnCountConnectionType(SocketStreamMetrics::SSL_CONNECTION);
  return connection_->socket()->Connect(io_callback_);
}

int SocketStream::DoSSLConnectComplete(int result) {
  DCHECK_EQ(STATE_NONE, next_state_);
  // Reconnect with client authentication.
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED)
    return HandleCertificateRequest(result, &server_ssl_config_);

  if (IsCertificateError(result))
    next_state_ = STATE_SSL_HANDLE_CERT_ERROR;
  else if (result == OK)
    result = DidEstablishConnection();
  else
    next_state_ = STATE_CLOSE;
  return result;
}

int SocketStream::DoSSLHandleCertError(int result) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(IsCertificateError(result));
  result = HandleCertificateError(result);
  if (result == OK || result == ERR_IO_PENDING)
    next_state_ = STATE_SSL_HANDLE_CERT_ERROR_COMPLETE;
  else
    next_state_ = STATE_CLOSE;
  return result;
}

int SocketStream::DoSSLHandleCertErrorComplete(int result) {
  DCHECK_EQ(STATE_NONE, next_state_);
  // TODO(toyoshim): Upgrade to SPDY through TLS NPN extension if possible.
  // If we use HTTPS and this is the first connection to the SPDY server,
  // we should take care of TLS NPN extension here.

  if (result == OK) {
    if (!connection_->socket()->IsConnectedAndIdle())
      return AllowCertErrorForReconnection(&server_ssl_config_);
    result = DidEstablishConnection();
  } else {
    next_state_ = STATE_CLOSE;
  }
  return result;
}

int SocketStream::DoReadWrite(int result) {
  if (result < OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }
  if (!connection_->socket() || !connection_->socket()->IsConnected()) {
    next_state_ = STATE_CLOSE;
    return ERR_CONNECTION_CLOSED;
  }

  // If client has requested close(), and there's nothing to write, then
  // let's close the socket.
  // We don't care about receiving data after the socket is closed.
  if (closing_ && !current_write_buf_.get() && pending_write_bufs_.empty()) {
    connection_->socket()->Disconnect();
    next_state_ = STATE_CLOSE;
    return OK;
  }

  next_state_ = STATE_READ_WRITE;

  // If server already closed the socket, we don't try to read.
  if (!server_closed_) {
    if (!read_buf_.get()) {
      // No read pending and server didn't close the socket.
      read_buf_ = new IOBuffer(kReadBufferSize);
      result = connection_->socket()->Read(
          read_buf_.get(),
          kReadBufferSize,
          base::Bind(&SocketStream::OnReadCompleted, base::Unretained(this)));
      if (result > 0) {
        return DidReceiveData(result);
      } else if (result == 0) {
        // 0 indicates end-of-file, so socket was closed.
        next_state_ = STATE_CLOSE;
        server_closed_ = true;
        return ERR_CONNECTION_CLOSED;
      }
      // If read is pending, try write as well.
      // Otherwise, return the result and do next loop (to close the
      // connection).
      if (result != ERR_IO_PENDING) {
        next_state_ = STATE_CLOSE;
        server_closed_ = true;
        return result;
      }
    }
    // Read is pending.
    DCHECK(read_buf_.get());
  }

  if (waiting_for_write_completion_)
    return ERR_IO_PENDING;

  if (!current_write_buf_.get()) {
    if (pending_write_bufs_.empty()) {
      // Nothing buffered for send.
      return ERR_IO_PENDING;
    }

    current_write_buf_ = new DrainableIOBuffer(
        pending_write_bufs_.front().get(), pending_write_bufs_.front()->size());
    pending_write_bufs_.pop_front();
  }

  result = connection_->socket()->Write(
      current_write_buf_.get(),
      current_write_buf_->BytesRemaining(),
      base::Bind(&SocketStream::OnWriteCompleted, base::Unretained(this)));

  if (result == ERR_IO_PENDING) {
    waiting_for_write_completion_ = true;
  } else if (result < 0) {
    // Shortcut. Enter STATE_CLOSE now by changing next_state_ here than by
    // calling DoReadWrite() again with the error code.
    next_state_ = STATE_CLOSE;
  } else if (result > 0) {
    // Write is not pending. Return OK and do next loop.
    DidSendData(result);
    result = OK;
  }

  return result;
}

GURL SocketStream::ProxyAuthOrigin() const {
  DCHECK(!proxy_info_.is_empty());
  return GURL("http://" +
              proxy_info_.proxy_server().host_port_pair().ToString());
}

int SocketStream::HandleCertificateRequest(int result, SSLConfig* ssl_config) {
  if (ssl_config->send_client_cert) {
    // We already have performed SSL client authentication once and failed.
    return result;
  }

  DCHECK(connection_->socket());
  scoped_refptr<SSLCertRequestInfo> cert_request_info = new SSLCertRequestInfo;
  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  ssl_socket->GetSSLCertRequestInfo(cert_request_info.get());

  HttpTransactionFactory* factory = context_->http_transaction_factory();
  if (!factory)
    return result;
  scoped_refptr<HttpNetworkSession> session = factory->GetSession();
  if (!session.get())
    return result;

  // If the user selected one of the certificates in client_certs or declined
  // to provide one for this server before, use the past decision
  // automatically.
  scoped_refptr<X509Certificate> client_cert;
  if (!session->ssl_client_auth_cache()->Lookup(
          cert_request_info->host_and_port, &client_cert)) {
    return result;
  }

  // Note: |client_cert| may be NULL, indicating that the caller
  // wishes to proceed anonymously (eg: continue the handshake
  // without sending a client cert)
  //
  // Check that the certificate selected is still a certificate the server
  // is likely to accept, based on the criteria supplied in the
  // CertificateRequest message.
  const std::vector<std::string>& cert_authorities =
      cert_request_info->cert_authorities;
  if (client_cert.get() && !cert_authorities.empty() &&
      !client_cert->IsIssuedByEncoded(cert_authorities)) {
    return result;
  }

  ssl_config->send_client_cert = true;
  ssl_config->client_cert = client_cert;
  next_state_ = STATE_TCP_CONNECT;
  return OK;
}

int SocketStream::AllowCertErrorForReconnection(SSLConfig* ssl_config) {
  DCHECK(ssl_config);
  // The SSL handshake didn't finish, or the server closed the SSL connection.
  // So, we should restart establishing connection with the certificate in
  // allowed bad certificates in |ssl_config|.
  // See also net/http/http_network_transaction.cc HandleCertificateError() and
  // RestartIgnoringLastError().
  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  SSLInfo ssl_info;
  ssl_socket->GetSSLInfo(&ssl_info);
  if (ssl_info.cert.get() == NULL ||
      ssl_config->IsAllowedBadCert(ssl_info.cert.get(), NULL)) {
    // If we already have the certificate in the set of allowed bad
    // certificates, we did try it and failed again, so we should not
    // retry again: the connection should fail at last.
    next_state_ = STATE_CLOSE;
    return ERR_UNEXPECTED;
  }
  // Add the bad certificate to the set of allowed certificates in the
  // SSL config object.
  SSLConfig::CertAndStatus bad_cert;
  if (!X509Certificate::GetDEREncoded(ssl_info.cert->os_cert_handle(),
                                      &bad_cert.der_cert)) {
    next_state_ = STATE_CLOSE;
    return ERR_UNEXPECTED;
  }
  bad_cert.cert_status = ssl_info.cert_status;
  ssl_config->allowed_bad_certs.push_back(bad_cert);
  // Restart connection ignoring the bad certificate.
  connection_->socket()->Disconnect();
  connection_->SetSocket(scoped_ptr<StreamSocket>());
  next_state_ = STATE_TCP_CONNECT;
  return OK;
}

void SocketStream::DoAuthRequired() {
  if (delegate_ && proxy_auth_controller_.get())
    delegate_->OnAuthRequired(this, proxy_auth_controller_->auth_info().get());
  else
    DoLoop(ERR_UNEXPECTED);
}

void SocketStream::DoRestartWithAuth() {
  DCHECK_EQ(next_state_, STATE_AUTH_REQUIRED);
  tunnel_request_headers_ = NULL;
  tunnel_request_headers_bytes_sent_ = 0;
  tunnel_response_headers_ = NULL;
  tunnel_response_headers_capacity_ = 0;
  tunnel_response_headers_len_ = 0;

  next_state_ = STATE_TCP_CONNECT;
  DoLoop(OK);
}

int SocketStream::HandleCertificateError(int result) {
  DCHECK(IsCertificateError(result));
  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  DCHECK(ssl_socket);

  if (!context_)
    return result;

  if (SSLClientSocket::IgnoreCertError(result, LOAD_IGNORE_ALL_CERT_ERRORS)) {
    const HttpNetworkSession::Params* session_params =
        context_->GetNetworkSessionParams();
    if (session_params && session_params->ignore_certificate_errors)
      return OK;
  }

  if (!delegate_)
    return result;

  SSLInfo ssl_info;
  ssl_socket->GetSSLInfo(&ssl_info);

  TransportSecurityState* state = context_->transport_security_state();
  const bool fatal =
      state &&
      state->ShouldSSLErrorsBeFatal(
          url_.host(),
          SSLConfigService::IsSNIAvailable(context_->ssl_config_service()));

  delegate_->OnSSLCertificateError(this, ssl_info, fatal);
  return ERR_IO_PENDING;
}

CookieStore* SocketStream::cookie_store() const {
  return cookie_store_;
}

}  // namespace net
