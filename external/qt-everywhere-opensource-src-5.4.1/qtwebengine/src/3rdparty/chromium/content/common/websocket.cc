// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/websocket.h"

namespace content {

WebSocketHandshakeRequest::WebSocketHandshakeRequest() {}

WebSocketHandshakeRequest::~WebSocketHandshakeRequest() {}

WebSocketHandshakeResponse::WebSocketHandshakeResponse()
    : status_code(0) {}

WebSocketHandshakeResponse::~WebSocketHandshakeResponse() {}

}  // namespace content
