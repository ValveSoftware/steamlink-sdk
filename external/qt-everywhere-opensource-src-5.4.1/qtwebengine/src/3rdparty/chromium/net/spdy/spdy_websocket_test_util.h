// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_WEBSOCKET_TEST_UTIL_H_
#define NET_SPDY_SPDY_WEBSOCKET_TEST_UTIL_H_

#include "net/base/request_priority.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_util_common.h"

namespace net {

class SpdyWebSocketTestUtil {
 public:
  explicit SpdyWebSocketTestUtil(NextProto protocol);

  // Returns the value corresponding to the given key (passed through
  // GetHeaderKey()), or the empty string if none exists.
  std::string GetHeader(const SpdyHeaderBlock& headers,
                        const std::string& key) const;

  // Adds the given key/value pair to |headers|, passing the key
  // through GetHeaderKey().
  void SetHeader(const std::string& key,
                 const std::string& value,
                 SpdyHeaderBlock* headers) const;

  // Constructs a standard SPDY SYN_STREAM frame for WebSocket over
  // SPDY opening handshake.
  SpdyFrame* ConstructSpdyWebSocketSynStream(int stream_id,
                                             const char* path,
                                             const char* host,
                                             const char* origin);

  // Constructs a standard SPDY SYN_REPLY packet to match the
  // WebSocket over SPDY opening handshake.
  SpdyFrame* ConstructSpdyWebSocketSynReply(int stream_id);

  // Constructs a WebSocket over SPDY handshake request packet.
  SpdyFrame* ConstructSpdyWebSocketHandshakeRequestFrame(
      scoped_ptr<SpdyHeaderBlock> headers,
      SpdyStreamId stream_id,
      RequestPriority request_priority);

  // Constructs a WebSocket over SPDY handshake response packet.
  SpdyFrame* ConstructSpdyWebSocketHandshakeResponseFrame(
      scoped_ptr<SpdyHeaderBlock> headers,
      SpdyStreamId stream_id,
      RequestPriority request_priority);

  // Constructs a SPDY HEADERS frame for a WebSocket frame over SPDY.
  SpdyFrame* ConstructSpdyWebSocketHeadersFrame(int stream_id,
                                                const char* length,
                                                bool fin);

  // Constructs a WebSocket over SPDY data packet.
  SpdyFrame* ConstructSpdyWebSocketDataFrame(const char* data,
                                             int len,
                                             SpdyStreamId stream_id,
                                             bool fin);

  // Forwards to |spdy_util_|.
  SpdyFrame* ConstructSpdySettings(const SettingsMap& settings) const;
  SpdyFrame* ConstructSpdySettingsAck() const;
  SpdyMajorVersion spdy_version() const;

 private:
  // Modify the header key based on the SPDY version and return it.
  std::string GetHeaderKey(const std::string& key) const;

  SpdyTestUtil spdy_util_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_WEBSOCKET_TEST_UTIL_H_
