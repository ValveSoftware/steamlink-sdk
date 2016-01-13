// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_H_
#define NET_SERVER_WEB_SOCKET_H_

#include <string>

#include "base/basictypes.h"

namespace net {

class HttpConnection;
class HttpServerRequestInfo;

class WebSocket {
 public:
  enum ParseResult {
    FRAME_OK,
    FRAME_INCOMPLETE,
    FRAME_CLOSE,
    FRAME_ERROR
  };

  static WebSocket* CreateWebSocket(HttpConnection* connection,
                                    const HttpServerRequestInfo& request,
                                    size_t* pos);

  static ParseResult DecodeFrameHybi17(const std::string& frame,
                                       bool client_frame,
                                       int* bytes_consumed,
                                       std::string* output);

  static std::string EncodeFrameHybi17(const std::string& data,
                                       int masking_key);

  virtual void Accept(const HttpServerRequestInfo& request) = 0;
  virtual ParseResult Read(std::string* message) = 0;
  virtual void Send(const std::string& message) = 0;
  virtual ~WebSocket() {}

 protected:
  explicit WebSocket(HttpConnection* connection);
  HttpConnection* connection_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_H_
