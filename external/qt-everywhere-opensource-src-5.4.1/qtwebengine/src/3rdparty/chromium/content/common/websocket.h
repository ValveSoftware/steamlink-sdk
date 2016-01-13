// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEBSOCKET_H_
#define CONTENT_COMMON_WEBSOCKET_H_

#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace content {

// WebSocket data message types sent between the browser and renderer processes.
enum WebSocketMessageType {
  WEB_SOCKET_MESSAGE_TYPE_CONTINUATION = 0x0,
  WEB_SOCKET_MESSAGE_TYPE_TEXT = 0x1,
  WEB_SOCKET_MESSAGE_TYPE_BINARY = 0x2,
  WEB_SOCKET_MESSAGE_TYPE_LAST = WEB_SOCKET_MESSAGE_TYPE_BINARY
};

// Opening handshake request information which will be shown in the inspector.
// All string data should be encoded to ASCII in the browser process.
struct WebSocketHandshakeRequest {
  WebSocketHandshakeRequest();
  ~WebSocketHandshakeRequest();

  // The request URL
  GURL url;
  // Additional HTTP request headers
  std::vector<std::pair<std::string, std::string> > headers;
  // HTTP request headers raw string
  std::string headers_text;
  // The time that this request is sent
  base::Time request_time;
};

// Opening handshake response information which will be shown in the inspector.
// All string data should be encoded to ASCII in the browser process.
struct WebSocketHandshakeResponse {
  WebSocketHandshakeResponse();
  ~WebSocketHandshakeResponse();

  // The request URL
  GURL url;
  // HTTP status code
  int status_code;
  // HTTP status text
  std::string status_text;
  // Additional HTTP response headers
  std::vector<std::pair<std::string, std::string> > headers;
  // HTTP response headers raw string
  std::string headers_text;
  // The time that this response arrives
  base::Time response_time;
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBSOCKET_H_
