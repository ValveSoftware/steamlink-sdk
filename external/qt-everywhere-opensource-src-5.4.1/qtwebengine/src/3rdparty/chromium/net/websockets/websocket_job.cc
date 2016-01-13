// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_handshake_handler.h"
#include "net/websockets/websocket_net_log_params.h"
#include "net/websockets/websocket_throttle.h"
#include "url/gurl.h"

static const int kMaxPendingSendAllowed = 32768;  // 32 kilobytes.

namespace {

// lower-case header names.
const char* const kCookieHeaders[] = {
  "cookie", "cookie2"
};
const char* const kSetCookieHeaders[] = {
  "set-cookie", "set-cookie2"
};

net::SocketStreamJob* WebSocketJobFactory(
    const GURL& url, net::SocketStream::Delegate* delegate,
    net::URLRequestContext* context, net::CookieStore* cookie_store) {
  net::WebSocketJob* job = new net::WebSocketJob(delegate);
  job->InitSocketStream(new net::SocketStream(url, job, context, cookie_store));
  return job;
}

class WebSocketJobInitSingleton {
 private:
  friend struct base::DefaultLazyInstanceTraits<WebSocketJobInitSingleton>;
  WebSocketJobInitSingleton() {
    net::SocketStreamJob::RegisterProtocolFactory("ws", WebSocketJobFactory);
    net::SocketStreamJob::RegisterProtocolFactory("wss", WebSocketJobFactory);
  }
};

static base::LazyInstance<WebSocketJobInitSingleton> g_websocket_job_init =
    LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

namespace net {

// static
void WebSocketJob::EnsureInit() {
  g_websocket_job_init.Get();
}

WebSocketJob::WebSocketJob(SocketStream::Delegate* delegate)
    : delegate_(delegate),
      state_(INITIALIZED),
      waiting_(false),
      handshake_request_(new WebSocketHandshakeRequestHandler),
      handshake_response_(new WebSocketHandshakeResponseHandler),
      started_to_send_handshake_request_(false),
      handshake_request_sent_(0),
      response_cookies_save_index_(0),
      spdy_protocol_version_(0),
      save_next_cookie_running_(false),
      callback_pending_(false),
      weak_ptr_factory_(this),
      weak_ptr_factory_for_send_pending_(this) {
}

WebSocketJob::~WebSocketJob() {
  DCHECK_EQ(CLOSED, state_);
  DCHECK(!delegate_);
  DCHECK(!socket_.get());
}

void WebSocketJob::Connect() {
  DCHECK(socket_.get());
  DCHECK_EQ(state_, INITIALIZED);
  state_ = CONNECTING;
  socket_->Connect();
}

bool WebSocketJob::SendData(const char* data, int len) {
  switch (state_) {
    case INITIALIZED:
      return false;

    case CONNECTING:
      return SendHandshakeRequest(data, len);

    case OPEN:
      {
        scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(len);
        memcpy(buffer->data(), data, len);
        if (current_send_buffer_.get() || !send_buffer_queue_.empty()) {
          send_buffer_queue_.push_back(buffer);
          return true;
        }
        current_send_buffer_ = new DrainableIOBuffer(buffer.get(), len);
        return SendDataInternal(current_send_buffer_->data(),
                                current_send_buffer_->BytesRemaining());
      }

    case CLOSING:
    case CLOSED:
      return false;
  }
  return false;
}

void WebSocketJob::Close() {
  if (state_ == CLOSED)
    return;

  state_ = CLOSING;
  if (current_send_buffer_.get()) {
    // Will close in SendPending.
    return;
  }
  state_ = CLOSED;
  CloseInternal();
}

void WebSocketJob::RestartWithAuth(const AuthCredentials& credentials) {
  state_ = CONNECTING;
  socket_->RestartWithAuth(credentials);
}

void WebSocketJob::DetachDelegate() {
  state_ = CLOSED;
  WebSocketThrottle::GetInstance()->RemoveFromQueue(this);

  scoped_refptr<WebSocketJob> protect(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_for_send_pending_.InvalidateWeakPtrs();

  delegate_ = NULL;
  if (socket_.get())
    socket_->DetachDelegate();
  socket_ = NULL;
  if (!callback_.is_null()) {
    waiting_ = false;
    callback_.Reset();
    Release();  // Balanced with OnStartOpenConnection().
  }
}

int WebSocketJob::OnStartOpenConnection(
    SocketStream* socket, const CompletionCallback& callback) {
  DCHECK(callback_.is_null());
  state_ = CONNECTING;

  addresses_ = socket->address_list();
  if (!WebSocketThrottle::GetInstance()->PutInQueue(this)) {
    return ERR_WS_THROTTLE_QUEUE_TOO_LARGE;
  }

  if (delegate_) {
    int result = delegate_->OnStartOpenConnection(socket, callback);
    DCHECK_EQ(OK, result);
  }
  if (waiting_) {
    // PutInQueue() may set |waiting_| true for throttling. In this case,
    // Wakeup() will be called later.
    callback_ = callback;
    AddRef();  // Balanced when callback_ is cleared.
    return ERR_IO_PENDING;
  }
  return TrySpdyStream();
}

void WebSocketJob::OnConnected(
    SocketStream* socket, int max_pending_send_allowed) {
  if (state_ == CLOSED)
    return;
  DCHECK_EQ(CONNECTING, state_);
  if (delegate_)
    delegate_->OnConnected(socket, max_pending_send_allowed);
}

void WebSocketJob::OnSentData(SocketStream* socket, int amount_sent) {
  DCHECK_NE(INITIALIZED, state_);
  DCHECK_GT(amount_sent, 0);
  if (state_ == CLOSED)
    return;
  if (state_ == CONNECTING) {
    OnSentHandshakeRequest(socket, amount_sent);
    return;
  }
  if (delegate_) {
    DCHECK(state_ == OPEN || state_ == CLOSING);
    if (!current_send_buffer_.get()) {
      VLOG(1)
          << "OnSentData current_send_buffer=NULL amount_sent=" << amount_sent;
      return;
    }
    current_send_buffer_->DidConsume(amount_sent);
    if (current_send_buffer_->BytesRemaining() > 0)
      return;

    // We need to report amount_sent of original buffer size, instead of
    // amount sent to |socket|.
    amount_sent = current_send_buffer_->size();
    DCHECK_GT(amount_sent, 0);
    current_send_buffer_ = NULL;
    if (!weak_ptr_factory_for_send_pending_.HasWeakPtrs()) {
      base::MessageLoopForIO::current()->PostTask(
          FROM_HERE,
          base::Bind(&WebSocketJob::SendPending,
                     weak_ptr_factory_for_send_pending_.GetWeakPtr()));
    }
    delegate_->OnSentData(socket, amount_sent);
  }
}

void WebSocketJob::OnReceivedData(
    SocketStream* socket, const char* data, int len) {
  DCHECK_NE(INITIALIZED, state_);
  if (state_ == CLOSED)
    return;
  if (state_ == CONNECTING) {
    OnReceivedHandshakeResponse(socket, data, len);
    return;
  }
  DCHECK(state_ == OPEN || state_ == CLOSING);
  if (delegate_ && len > 0)
    delegate_->OnReceivedData(socket, data, len);
}

void WebSocketJob::OnClose(SocketStream* socket) {
  state_ = CLOSED;
  WebSocketThrottle::GetInstance()->RemoveFromQueue(this);

  scoped_refptr<WebSocketJob> protect(this);
  weak_ptr_factory_.InvalidateWeakPtrs();

  SocketStream::Delegate* delegate = delegate_;
  delegate_ = NULL;
  socket_ = NULL;
  if (!callback_.is_null()) {
    waiting_ = false;
    callback_.Reset();
    Release();  // Balanced with OnStartOpenConnection().
  }
  if (delegate)
    delegate->OnClose(socket);
}

void WebSocketJob::OnAuthRequired(
    SocketStream* socket, AuthChallengeInfo* auth_info) {
  if (delegate_)
    delegate_->OnAuthRequired(socket, auth_info);
}

void WebSocketJob::OnSSLCertificateError(
    SocketStream* socket, const SSLInfo& ssl_info, bool fatal) {
  if (delegate_)
    delegate_->OnSSLCertificateError(socket, ssl_info, fatal);
}

void WebSocketJob::OnError(const SocketStream* socket, int error) {
  if (delegate_ && error != ERR_PROTOCOL_SWITCHED)
    delegate_->OnError(socket, error);
}

void WebSocketJob::OnCreatedSpdyStream(int result) {
  DCHECK(spdy_websocket_stream_.get());
  DCHECK(socket_.get());
  DCHECK_NE(ERR_IO_PENDING, result);

  if (state_ == CLOSED) {
    result = ERR_ABORTED;
  } else if (result == OK) {
    state_ = CONNECTING;
    result = ERR_PROTOCOL_SWITCHED;
  } else {
    spdy_websocket_stream_.reset();
  }

  CompleteIO(result);
}

void WebSocketJob::OnSentSpdyHeaders() {
  DCHECK_NE(INITIALIZED, state_);
  if (state_ != CONNECTING)
    return;
  size_t original_length = handshake_request_->original_length();
  handshake_request_.reset();
  if (delegate_)
    delegate_->OnSentData(socket_.get(), original_length);
}

void WebSocketJob::OnSpdyResponseHeadersUpdated(
    const SpdyHeaderBlock& response_headers) {
  DCHECK_NE(INITIALIZED, state_);
  if (state_ != CONNECTING)
    return;
  // TODO(toyoshim): Fallback to non-spdy connection?
  handshake_response_->ParseResponseHeaderBlock(response_headers,
                                                challenge_,
                                                spdy_protocol_version_);

  SaveCookiesAndNotifyHeadersComplete();
}

void WebSocketJob::OnSentSpdyData(size_t bytes_sent) {
  DCHECK_NE(INITIALIZED, state_);
  DCHECK_NE(CONNECTING, state_);
  if (state_ == CLOSED)
    return;
  if (!spdy_websocket_stream_.get())
    return;
  OnSentData(socket_.get(), static_cast<int>(bytes_sent));
}

void WebSocketJob::OnReceivedSpdyData(scoped_ptr<SpdyBuffer> buffer) {
  DCHECK_NE(INITIALIZED, state_);
  DCHECK_NE(CONNECTING, state_);
  if (state_ == CLOSED)
    return;
  if (!spdy_websocket_stream_.get())
    return;
  if (buffer) {
    OnReceivedData(
        socket_.get(), buffer->GetRemainingData(), buffer->GetRemainingSize());
  } else {
    OnReceivedData(socket_.get(), NULL, 0);
  }
}

void WebSocketJob::OnCloseSpdyStream() {
  spdy_websocket_stream_.reset();
  OnClose(socket_.get());
}

bool WebSocketJob::SendHandshakeRequest(const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  if (started_to_send_handshake_request_)
    return false;
  if (!handshake_request_->ParseRequest(data, len))
    return false;

  AddCookieHeaderAndSend();
  return true;
}

void WebSocketJob::AddCookieHeaderAndSend() {
  bool allow = true;
  if (delegate_ && !delegate_->CanGetCookies(socket_.get(), GetURLForCookies()))
    allow = false;

  if (socket_.get() && delegate_ && state_ == CONNECTING) {
    handshake_request_->RemoveHeaders(kCookieHeaders,
                                      arraysize(kCookieHeaders));
    if (allow && socket_->cookie_store()) {
      // Add cookies, including HttpOnly cookies.
      CookieOptions cookie_options;
      cookie_options.set_include_httponly();
      socket_->cookie_store()->GetCookiesWithOptionsAsync(
          GetURLForCookies(), cookie_options,
          base::Bind(&WebSocketJob::LoadCookieCallback,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      DoSendData();
    }
  }
}

void WebSocketJob::LoadCookieCallback(const std::string& cookie) {
  if (!cookie.empty())
    // TODO(tyoshino): Sending cookie means that connection doesn't need
    // PRIVACY_MODE_ENABLED as cookies may be server-bound and channel id
    // wouldn't negatively affect privacy anyway. Need to restart connection
    // or refactor to determine cookie status prior to connecting.
    handshake_request_->AppendHeaderIfMissing("Cookie", cookie);
  DoSendData();
}

void WebSocketJob::DoSendData() {
  if (spdy_websocket_stream_.get()) {
    scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
    handshake_request_->GetRequestHeaderBlock(
        socket_->url(), headers.get(), &challenge_, spdy_protocol_version_);
    spdy_websocket_stream_->SendRequest(headers.Pass());
  } else {
    const std::string& handshake_request =
        handshake_request_->GetRawRequest();
    handshake_request_sent_ = 0;
    socket_->net_log()->AddEvent(
        NetLog::TYPE_WEB_SOCKET_SEND_REQUEST_HEADERS,
        base::Bind(&NetLogWebSocketHandshakeCallback, &handshake_request));
    socket_->SendData(handshake_request.data(),
                      handshake_request.size());
  }
  // Just buffered in |handshake_request_|.
  started_to_send_handshake_request_ = true;
}

void WebSocketJob::OnSentHandshakeRequest(
    SocketStream* socket, int amount_sent) {
  DCHECK_EQ(state_, CONNECTING);
  handshake_request_sent_ += amount_sent;
  DCHECK_LE(handshake_request_sent_, handshake_request_->raw_length());
  if (handshake_request_sent_ >= handshake_request_->raw_length()) {
    // handshake request has been sent.
    // notify original size of handshake request to delegate.
    // Reset the handshake_request_ first in case this object is deleted by the
    // delegate.
    size_t original_length = handshake_request_->original_length();
    handshake_request_.reset();
    if (delegate_)
      delegate_->OnSentData(socket, original_length);
  }
}

void WebSocketJob::OnReceivedHandshakeResponse(
    SocketStream* socket, const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  if (handshake_response_->HasResponse()) {
    // If we already has handshake response, received data should be frame
    // data, not handshake message.
    received_data_after_handshake_.insert(
        received_data_after_handshake_.end(), data, data + len);
    return;
  }

  size_t response_length = handshake_response_->ParseRawResponse(data, len);
  if (!handshake_response_->HasResponse()) {
    // not yet. we need more data.
    return;
  }
  // handshake message is completed.
  std::string raw_response = handshake_response_->GetRawResponse();
  socket_->net_log()->AddEvent(
      NetLog::TYPE_WEB_SOCKET_READ_RESPONSE_HEADERS,
      base::Bind(&NetLogWebSocketHandshakeCallback, &raw_response));
  if (len - response_length > 0) {
    // If we received extra data, it should be frame data.
    DCHECK(received_data_after_handshake_.empty());
    received_data_after_handshake_.assign(data + response_length, data + len);
  }
  SaveCookiesAndNotifyHeadersComplete();
}

void WebSocketJob::SaveCookiesAndNotifyHeadersComplete() {
  // handshake message is completed.
  DCHECK(handshake_response_->HasResponse());

  // Extract cookies from the handshake response into a temporary vector.
  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  handshake_response_->GetHeaders(
      kSetCookieHeaders, arraysize(kSetCookieHeaders), &response_cookies_);

  // Now, loop over the response cookies, and attempt to persist each.
  SaveNextCookie();
}

void WebSocketJob::NotifyHeadersComplete() {
  // Remove cookie headers, with malformed headers preserved.
  // Actual handshake should be done in Blink.
  handshake_response_->RemoveHeaders(
      kSetCookieHeaders, arraysize(kSetCookieHeaders));
  std::string handshake_response = handshake_response_->GetResponse();
  handshake_response_.reset();
  std::vector<char> received_data(handshake_response.begin(),
                                  handshake_response.end());
  received_data.insert(received_data.end(),
                       received_data_after_handshake_.begin(),
                       received_data_after_handshake_.end());
  received_data_after_handshake_.clear();

  state_ = OPEN;

  DCHECK(!received_data.empty());
  if (delegate_)
    delegate_->OnReceivedData(
        socket_.get(), &received_data.front(), received_data.size());

  WebSocketThrottle::GetInstance()->RemoveFromQueue(this);
}

void WebSocketJob::SaveNextCookie() {
  if (!socket_.get() || !delegate_ || state_ != CONNECTING)
    return;

  callback_pending_ = false;
  save_next_cookie_running_ = true;

  if (socket_->cookie_store()) {
    GURL url_for_cookies = GetURLForCookies();

    CookieOptions options;
    options.set_include_httponly();

    // Loop as long as SetCookieWithOptionsAsync completes synchronously. Since
    // CookieMonster's asynchronous operation APIs queue the callback to run it
    // on the thread where the API was called, there won't be race. I.e. unless
    // the callback is run synchronously, it won't be run in parallel with this
    // method.
    while (!callback_pending_ &&
           response_cookies_save_index_ < response_cookies_.size()) {
      std::string cookie = response_cookies_[response_cookies_save_index_];
      response_cookies_save_index_++;

      if (!delegate_->CanSetCookie(
              socket_.get(), url_for_cookies, cookie, &options))
        continue;

      callback_pending_ = true;
      socket_->cookie_store()->SetCookieWithOptionsAsync(
          url_for_cookies, cookie, options,
          base::Bind(&WebSocketJob::OnCookieSaved,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  save_next_cookie_running_ = false;

  if (callback_pending_)
    return;

  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  NotifyHeadersComplete();
}

void WebSocketJob::OnCookieSaved(bool cookie_status) {
  // Tell the caller of SetCookieWithOptionsAsync() that this completion
  // callback is invoked.
  // - If the caller checks callback_pending earlier than this callback, the
  //   caller exits to let this method continue iteration.
  // - Otherwise, the caller continues iteration.
  callback_pending_ = false;

  // Resume SaveNextCookie if the caller of SetCookieWithOptionsAsync() exited
  // the loop. Otherwise, return.
  if (save_next_cookie_running_)
    return;

  SaveNextCookie();
}

GURL WebSocketJob::GetURLForCookies() const {
  GURL url = socket_->url();
  std::string scheme = socket_->is_secure() ? "https" : "http";
  url::Replacements<char> replacements;
  replacements.SetScheme(scheme.c_str(), url::Component(0, scheme.length()));
  return url.ReplaceComponents(replacements);
}

const AddressList& WebSocketJob::address_list() const {
  return addresses_;
}

int WebSocketJob::TrySpdyStream() {
  if (!socket_.get())
    return ERR_FAILED;

  // Check if we have a SPDY session available.
  HttpTransactionFactory* factory =
      socket_->context()->http_transaction_factory();
  if (!factory)
    return OK;
  scoped_refptr<HttpNetworkSession> session = factory->GetSession();
  if (!session.get() || !session->params().enable_websocket_over_spdy)
    return OK;
  SpdySessionPool* spdy_pool = session->spdy_session_pool();
  PrivacyMode privacy_mode = socket_->privacy_mode();
  const SpdySessionKey key(HostPortPair::FromURL(socket_->url()),
                           socket_->proxy_server(), privacy_mode);
  // Forbid wss downgrade to SPDY without SSL.
  // TODO(toyoshim): Does it realize the same policy with HTTP?
  base::WeakPtr<SpdySession> spdy_session =
      spdy_pool->FindAvailableSession(key, *socket_->net_log());
  if (!spdy_session)
    return OK;

  SSLInfo ssl_info;
  bool was_npn_negotiated;
  NextProto protocol_negotiated = kProtoUnknown;
  bool use_ssl = spdy_session->GetSSLInfo(
      &ssl_info, &was_npn_negotiated, &protocol_negotiated);
  if (socket_->is_secure() && !use_ssl)
    return OK;

  // Create SpdyWebSocketStream.
  spdy_protocol_version_ = spdy_session->GetProtocolVersion();
  spdy_websocket_stream_.reset(new SpdyWebSocketStream(spdy_session, this));

  int result = spdy_websocket_stream_->InitializeStream(
      socket_->url(), MEDIUM, *socket_->net_log());
  if (result == OK) {
    OnConnected(socket_.get(), kMaxPendingSendAllowed);
    return ERR_PROTOCOL_SWITCHED;
  }
  if (result != ERR_IO_PENDING) {
    spdy_websocket_stream_.reset();
    return OK;
  }

  return ERR_IO_PENDING;
}

void WebSocketJob::SetWaiting() {
  waiting_ = true;
}

bool WebSocketJob::IsWaiting() const {
  return waiting_;
}

void WebSocketJob::Wakeup() {
  if (!waiting_)
    return;
  waiting_ = false;
  DCHECK(!callback_.is_null());
  base::MessageLoopForIO::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketJob::RetryPendingIO,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebSocketJob::RetryPendingIO() {
  int result = TrySpdyStream();

  // In the case of ERR_IO_PENDING, CompleteIO() will be called from
  // OnCreatedSpdyStream().
  if (result != ERR_IO_PENDING)
    CompleteIO(result);
}

void WebSocketJob::CompleteIO(int result) {
  // |callback_| may be null if OnClose() or DetachDelegate() was called.
  if (!callback_.is_null()) {
    CompletionCallback callback = callback_;
    callback_.Reset();
    callback.Run(result);
    Release();  // Balanced with OnStartOpenConnection().
  }
}

bool WebSocketJob::SendDataInternal(const char* data, int length) {
  if (spdy_websocket_stream_.get())
    return ERR_IO_PENDING == spdy_websocket_stream_->SendData(data, length);
  if (socket_.get())
    return socket_->SendData(data, length);
  return false;
}

void WebSocketJob::CloseInternal() {
  if (spdy_websocket_stream_.get())
    spdy_websocket_stream_->Close();
  if (socket_.get())
    socket_->Close();
}

void WebSocketJob::SendPending() {
  if (current_send_buffer_.get())
    return;

  // Current buffer has been sent. Try next if any.
  if (send_buffer_queue_.empty()) {
    // No more data to send.
    if (state_ == CLOSING)
      CloseInternal();
    return;
  }

  scoped_refptr<IOBufferWithSize> next_buffer = send_buffer_queue_.front();
  send_buffer_queue_.pop_front();
  current_send_buffer_ =
      new DrainableIOBuffer(next_buffer.get(), next_buffer->size());
  SendDataInternal(current_send_buffer_->data(),
                   current_send_buffer_->BytesRemaining());
}

}  // namespace net
