// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "net/websockets/websocket_test_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

class StreamRequestImpl;

class Delegate : public URLRequest::Delegate {
 public:
  enum HandshakeResult {
    INCOMPLETE,
    CONNECTED,
    FAILED,
    NUM_HANDSHAKE_RESULT_TYPES,
  };

  explicit Delegate(StreamRequestImpl* owner)
      : owner_(owner), result_(INCOMPLETE) {}
  virtual ~Delegate() {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.WebSocket.HandshakeResult", result_, NUM_HANDSHAKE_RESULT_TYPES);
  }

  // Implementation of URLRequest::Delegate methods.
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE {
    // HTTP status codes returned by HttpStreamParser are filtered by
    // WebSocketBasicHandshakeStream, and only 101, 401 and 407 are permitted
    // back up the stack to HttpNetworkTransaction. In particular, redirect
    // codes are never allowed, and so URLRequest never sees a redirect on a
    // WebSocket request.
    NOTREACHED();
  }

  virtual void OnResponseStarted(URLRequest* request) OVERRIDE;

  virtual void OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info) OVERRIDE;

  virtual void OnCertificateRequested(URLRequest* request,
                                      SSLCertRequestInfo* cert_request_info)
      OVERRIDE;

  virtual void OnSSLCertificateError(URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) OVERRIDE;

 private:
  StreamRequestImpl* owner_;
  HandshakeResult result_;
};

class StreamRequestImpl : public WebSocketStreamRequest {
 public:
  StreamRequestImpl(
      const GURL& url,
      const URLRequestContext* context,
      const url::Origin& origin,
      scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate,
      scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper)
      : delegate_(new Delegate(this)),
        url_request_(url, DEFAULT_PRIORITY, delegate_.get(), context),
        connect_delegate_(connect_delegate.Pass()),
        create_helper_(create_helper.release()) {
    create_helper_->set_failure_message(&failure_message_);
    HttpRequestHeaders headers;
    headers.SetHeader(websockets::kUpgrade, websockets::kWebSocketLowercase);
    headers.SetHeader(HttpRequestHeaders::kConnection, websockets::kUpgrade);
    headers.SetHeader(HttpRequestHeaders::kOrigin, origin.string());
    headers.SetHeader(websockets::kSecWebSocketVersion,
                      websockets::kSupportedVersion);
    url_request_.SetExtraRequestHeaders(headers);

    // This passes the ownership of |create_helper_| to |url_request_|.
    url_request_.SetUserData(
        WebSocketHandshakeStreamBase::CreateHelper::DataKey(),
        create_helper_);
    url_request_.SetLoadFlags(LOAD_DISABLE_CACHE |
                              LOAD_BYPASS_CACHE |
                              LOAD_DO_NOT_PROMPT_FOR_LOGIN);
  }

  // Destroying this object destroys the URLRequest, which cancels the request
  // and so terminates the handshake if it is incomplete.
  virtual ~StreamRequestImpl() {}

  void Start() {
    url_request_.Start();
  }

  void PerformUpgrade() {
    connect_delegate_->OnSuccess(create_helper_->stream()->Upgrade());
  }

  void ReportFailure() {
    if (failure_message_.empty()) {
      switch (url_request_.status().status()) {
        case URLRequestStatus::SUCCESS:
        case URLRequestStatus::IO_PENDING:
          break;
        case URLRequestStatus::CANCELED:
          failure_message_ = "WebSocket opening handshake was canceled";
          break;
        case URLRequestStatus::FAILED:
          failure_message_ =
              std::string("Error in connection establishment: ") +
              ErrorToString(url_request_.status().error());
          break;
      }
    }
    connect_delegate_->OnFailure(failure_message_);
  }

  WebSocketStream::ConnectDelegate* connect_delegate() const {
    return connect_delegate_.get();
  }

 private:
  // |delegate_| needs to be declared before |url_request_| so that it gets
  // initialised first.
  scoped_ptr<Delegate> delegate_;

  // Deleting the StreamRequestImpl object deletes this URLRequest object,
  // cancelling the whole connection.
  URLRequest url_request_;

  scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;

  // Owned by the URLRequest.
  WebSocketHandshakeStreamCreateHelper* create_helper_;

  // The failure message supplied by WebSocketBasicHandshakeStream, if any.
  std::string failure_message_;
};

class SSLErrorCallbacks : public WebSocketEventInterface::SSLErrorCallbacks {
 public:
  explicit SSLErrorCallbacks(URLRequest* url_request)
      : url_request_(url_request) {}

  virtual void CancelSSLRequest(int error, const SSLInfo* ssl_info) OVERRIDE {
    if (ssl_info) {
      url_request_->CancelWithSSLError(error, *ssl_info);
    } else {
      url_request_->CancelWithError(error);
    }
  }

  virtual void ContinueSSLRequest() OVERRIDE {
    url_request_->ContinueDespiteLastError();
  }

 private:
  URLRequest* url_request_;
};

void Delegate::OnResponseStarted(URLRequest* request) {
  // All error codes, including OK and ABORTED, as with
  // Net.ErrorCodesForMainFrame3
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.WebSocket.ErrorCodes",
                              -request->status().error());
  if (!request->status().is_success()) {
    DVLOG(3) << "OnResponseStarted (request failed)";
    owner_->ReportFailure();
    return;
  }
  const int response_code = request->GetResponseCode();
  DVLOG(3) << "OnResponseStarted (response code " << response_code << ")";
  switch (response_code) {
    case HTTP_SWITCHING_PROTOCOLS:
      result_ = CONNECTED;
      owner_->PerformUpgrade();
      return;

    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return;

    default:
      result_ = FAILED;
      owner_->ReportFailure();
  }
}

void Delegate::OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info) {
  // This should only be called if credentials are not already stored.
  request->CancelAuth();
}

void Delegate::OnCertificateRequested(URLRequest* request,
                                      SSLCertRequestInfo* cert_request_info) {
  // This method is called when a client certificate is requested, and the
  // request context does not already contain a client certificate selection for
  // the endpoint. In this case, a main frame resource request would pop-up UI
  // to permit selection of a client certificate, but since WebSockets are
  // sub-resources they should not pop-up UI and so there is nothing more we can
  // do.
  request->Cancel();
}

void Delegate::OnSSLCertificateError(URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool fatal) {
  owner_->connect_delegate()->OnSSLCertificateError(
      scoped_ptr<WebSocketEventInterface::SSLErrorCallbacks>(
          new SSLErrorCallbacks(request)),
      ssl_info,
      fatal);
}

void Delegate::OnReadCompleted(URLRequest* request, int bytes_read) {
  NOTREACHED();
}

}  // namespace

WebSocketStreamRequest::~WebSocketStreamRequest() {}

WebSocketStream::WebSocketStream() {}
WebSocketStream::~WebSocketStream() {}

WebSocketStream::ConnectDelegate::~ConnectDelegate() {}

scoped_ptr<WebSocketStreamRequest> WebSocketStream::CreateAndConnectStream(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const url::Origin& origin,
    URLRequestContext* url_request_context,
    const BoundNetLog& net_log,
    scoped_ptr<ConnectDelegate> connect_delegate) {
  scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper(
      new WebSocketHandshakeStreamCreateHelper(connect_delegate.get(),
                                               requested_subprotocols));
  scoped_ptr<StreamRequestImpl> request(
      new StreamRequestImpl(socket_url,
                            url_request_context,
                            origin,
                            connect_delegate.Pass(),
                            create_helper.Pass()));
  request->Start();
  return request.PassAs<WebSocketStreamRequest>();
}

// This is declared in websocket_test_util.h.
scoped_ptr<WebSocketStreamRequest> CreateAndConnectStreamForTesting(
    const GURL& socket_url,
    scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper,
    const url::Origin& origin,
    URLRequestContext* url_request_context,
    const BoundNetLog& net_log,
    scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate) {
  scoped_ptr<StreamRequestImpl> request(
      new StreamRequestImpl(socket_url,
                            url_request_context,
                            origin,
                            connect_delegate.Pass(),
                            create_helper.Pass()));
  request->Start();
  return request.PassAs<WebSocketStreamRequest>();
}

}  // namespace net
