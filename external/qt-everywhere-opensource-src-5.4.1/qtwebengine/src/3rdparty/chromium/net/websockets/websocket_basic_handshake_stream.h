// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_HANDSHAKE_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_HANDSHAKE_STREAM_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/http/http_basic_state.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "url/gurl.h"

namespace net {

class ClientSocketHandle;
class HttpResponseHeaders;
class HttpResponseInfo;
class HttpStreamParser;

struct WebSocketExtensionParams;

class NET_EXPORT_PRIVATE WebSocketBasicHandshakeStream
    : public WebSocketHandshakeStreamBase {
 public:
  // |connect_delegate| and |failure_message| must out-live this object.
  WebSocketBasicHandshakeStream(
      scoped_ptr<ClientSocketHandle> connection,
      WebSocketStream::ConnectDelegate* connect_delegate,
      bool using_proxy,
      std::vector<std::string> requested_sub_protocols,
      std::vector<std::string> requested_extensions,
      std::string* failure_message);

  virtual ~WebSocketBasicHandshakeStream();

  // HttpStreamBase methods
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               RequestPriority priority,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) OVERRIDE;
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) OVERRIDE;
  virtual int ReadResponseHeaders(const CompletionCallback& callback) OVERRIDE;
  virtual int ReadResponseBody(IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback) OVERRIDE;
  virtual void Close(bool not_reusable) OVERRIDE;
  virtual bool IsResponseBodyComplete() const OVERRIDE;
  virtual bool CanFindEndOfResponse() const OVERRIDE;
  virtual bool IsConnectionReused() const OVERRIDE;
  virtual void SetConnectionReused() OVERRIDE;
  virtual bool IsConnectionReusable() const OVERRIDE;
  virtual int64 GetTotalReceivedBytes() const OVERRIDE;
  virtual bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const
      OVERRIDE;
  virtual void GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual bool IsSpdyHttpStream() const OVERRIDE;
  virtual void Drain(HttpNetworkSession* session) OVERRIDE;
  virtual void SetPriority(RequestPriority priority) OVERRIDE;

  // This is called from the top level once correct handshake response headers
  // have been received. It creates an appropriate subclass of WebSocketStream
  // depending on what extensions were negotiated. This object is unusable after
  // Upgrade() has been called and should be disposed of as soon as possible.
  virtual scoped_ptr<WebSocketStream> Upgrade() OVERRIDE;

  // Set the value used for the next Sec-WebSocket-Key header
  // deterministically. The key is only used once, and then discarded.
  // For tests only.
  void SetWebSocketKeyForTesting(const std::string& key);

 private:
  // A wrapper for the ReadResponseHeaders callback that checks whether or not
  // the connection has been accepted.
  void ReadResponseHeadersCallback(const CompletionCallback& callback,
                                   int result);

  void OnFinishOpeningHandshake();

  // Validates the response and sends the finished handshake event.
  int ValidateResponse(int rv);

  // Check that the headers are well-formed for a 101 response, and returns
  // OK if they are, otherwise returns ERR_INVALID_RESPONSE.
  int ValidateUpgradeResponse(const HttpResponseHeaders* headers);

  HttpStreamParser* parser() const { return state_.parser(); }

  void set_failure_message(const std::string& failure_message);

  // The request URL.
  GURL url_;

  // HttpBasicState holds most of the handshake-related state.
  HttpBasicState state_;

  // Owned by another object.
  // |connect_delegate| will live during the lifetime of this object.
  WebSocketStream::ConnectDelegate* connect_delegate_;

  // This is stored in SendRequest() for use by ReadResponseHeaders().
  HttpResponseInfo* http_response_info_;

  // The key to be sent in the next Sec-WebSocket-Key header. Usually NULL (the
  // key is generated on the fly).
  scoped_ptr<std::string> handshake_challenge_for_testing_;

  // The required value for the Sec-WebSocket-Accept header.
  std::string handshake_challenge_response_;

  // The sub-protocols we requested.
  std::vector<std::string> requested_sub_protocols_;

  // The extensions we requested.
  std::vector<std::string> requested_extensions_;

  // The sub-protocol selected by the server.
  std::string sub_protocol_;

  // The extension(s) selected by the server.
  std::string extensions_;

  // The extension parameters. The class is defined in the implementation file
  // to avoid including extension-related header files here.
  scoped_ptr<WebSocketExtensionParams> extension_params_;

  std::string* failure_message_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketBasicHandshakeStream);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_HANDSHAKE_STREAM_H_
