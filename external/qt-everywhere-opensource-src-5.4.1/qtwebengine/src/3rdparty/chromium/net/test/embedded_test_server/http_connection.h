// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "net/test/embedded_test_server/http_request.h"

namespace net {

class StreamListenSocket;

namespace test_server {

class HttpConnection;
class HttpResponse;

// Calblack called when a request is parsed. Response should be sent
// using HttpConnection::SendResponse() on the |connection| argument.
typedef base::Callback<void(HttpConnection* connection,
                            scoped_ptr<HttpRequest> request)>
    HandleRequestCallback;

// Wraps the connection socket. Accepts incoming data and sends responses.
// If a valid request is parsed, then |callback_| is invoked.
class HttpConnection {
 public:
  HttpConnection(scoped_ptr<StreamListenSocket> socket,
                 const HandleRequestCallback& callback);
  ~HttpConnection();

  // Sends the HTTP response to the client.
  void SendResponse(scoped_ptr<HttpResponse> response) const;

 private:
  friend class EmbeddedTestServer;

  // Accepts raw chunk of data from the client. Internally, passes it to the
  // HttpRequestParser class. If a request is parsed, then |callback_| is
  // called.
  void ReceiveData(const base::StringPiece& data);

  scoped_ptr<StreamListenSocket> socket_;
  const HandleRequestCallback callback_;
  HttpRequestParser request_parser_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_
