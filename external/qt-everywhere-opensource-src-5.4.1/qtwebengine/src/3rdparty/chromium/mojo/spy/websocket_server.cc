// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/spy/websocket_server.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_listen_socket.h"

namespace spy {

const int kNotConnected = -1;

WebSocketServer::WebSocketServer(int port)
    : port_(port), connection_id_(kNotConnected) {
}

WebSocketServer::~WebSocketServer() {
}

bool WebSocketServer::Start() {
  net::TCPListenSocketFactory factory("0.0.0.0", port_);
  server_ = new net::HttpServer(factory, this);
  net::IPEndPoint address;
  int error = server_->GetLocalAddress(&address);
  port_ = address.port();
  return (error == net::OK);
}

void WebSocketServer::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  server_->Send500(connection_id, "websockets protocol only");
}

void WebSocketServer::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  if (connection_id_ != kNotConnected) {
    // Reject connection since we already have our client.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::Close, server_, connection_id));
    return;
  }
  // Accept the connection.
  server_->AcceptWebSocket(connection_id, info);
  connection_id_ = connection_id;
}

void WebSocketServer::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  // TODO(cpu): remove this test code soon.
  if (data == "\"hello\"")
    server_->SendOverWebSocket(connection_id, "\"hi there!\"");
}

void WebSocketServer::OnClose(
    int connection_id) {
  if (connection_id == connection_id_)
    connection_id_ = kNotConnected;
}

}  // namespace spy
