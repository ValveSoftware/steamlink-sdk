// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
#define NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

NET_EXPORT_PRIVATE base::Value* NetLogWebSocketHandshakeCallback(
    const std::string* headers,
    NetLog::LogLevel /* log_level */);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
