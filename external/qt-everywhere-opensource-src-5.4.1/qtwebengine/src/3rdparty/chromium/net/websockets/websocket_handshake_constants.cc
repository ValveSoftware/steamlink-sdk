// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_constants.h"

namespace net {
namespace websockets {

const char* const kHttpProtocolVersion = "HTTP/1.1";

const size_t kRawChallengeLength = 16;

const char* const kSecWebSocketProtocol = "Sec-WebSocket-Protocol";
const char* const kSecWebSocketExtensions = "Sec-WebSocket-Extensions";
const char* const kSecWebSocketKey = "Sec-WebSocket-Key";
const char* const kSecWebSocketAccept = "Sec-WebSocket-Accept";
const char* const kSecWebSocketVersion = "Sec-WebSocket-Version";

const char* const kSupportedVersion = "13";

const char* const kUpgrade = "Upgrade";
const char* const kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

const char* const kSecWebSocketProtocolSpdy3 = ":sec-websocket-protocol";
const char* const kSecWebSocketExtensionsSpdy3 = ":sec-websocket-extensions";

const char* const kSecWebSocketProtocolLowercase =
    kSecWebSocketProtocolSpdy3 + 1;
const char* const kSecWebSocketExtensionsLowercase =
    kSecWebSocketExtensionsSpdy3 + 1;
const char* const kSecWebSocketKeyLowercase = "sec-websocket-key";
const char* const kSecWebSocketVersionLowercase = "sec-websocket-version";
const char* const kUpgradeLowercase = "upgrade";
const char* const kWebSocketLowercase = "websocket";

}  // namespace websockets
}  // namespace net
