// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SPY_WEBSOCKET_SERVER_H_
#define MOJO_SPY_WEBSOCKET_SERVER_H_

#include "net/server/http_server.h"

namespace spy {

class WebSocketServer : public net::HttpServer::Delegate {
 public:
  // Pass 0 in |port| to listen in one available port.
  explicit WebSocketServer(int port);
  virtual ~WebSocketServer();
  // Begin accepting HTTP requests. Must be called from an IO MessageLoop.
  bool Start();
  // Returns the listening port, useful if 0 was passed to the contructor.
  int port() const { return port_; }

 protected:
  // Overridden from net::HttpServer::Delegate.
  virtual void OnHttpRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(
      int connection_id,
      const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

 private:
  int port_;
  int connection_id_;
  scoped_refptr<net::HttpServer> server_;
  DISALLOW_COPY_AND_ASSIGN(WebSocketServer);
};

}  // namespace spy

#endif  // MOJO_SPY_WEBSOCKET_SERVER_H_
