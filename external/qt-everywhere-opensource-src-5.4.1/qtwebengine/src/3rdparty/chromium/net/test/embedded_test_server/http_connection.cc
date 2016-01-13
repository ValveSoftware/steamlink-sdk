// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_connection.h"

#include "net/socket/stream_listen_socket.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {

HttpConnection::HttpConnection(scoped_ptr<StreamListenSocket> socket,
                               const HandleRequestCallback& callback)
    : socket_(socket.Pass()),
      callback_(callback) {
}

HttpConnection::~HttpConnection() {
}

void HttpConnection::SendResponse(scoped_ptr<HttpResponse> response) const {
  const std::string response_string = response->ToResponseString();
  socket_->Send(response_string.c_str(), response_string.length());
}

void HttpConnection::ReceiveData(const base::StringPiece& data) {
  request_parser_.ProcessChunk(data);
  if (request_parser_.ParseRequest() == HttpRequestParser::ACCEPTED) {
    callback_.Run(this, request_parser_.GetRequest());
  }
}

}  // namespace test_server
}  // namespace net
