// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_test_util.h"

#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"

namespace net {

const int kDefaultAssociatedStreamId = 0;
const bool kDefaultCompressed = false;
const char* const kDefaultDataPointer = NULL;
const uint32 kDefaultDataLength = 0;

SpdyWebSocketTestUtil::SpdyWebSocketTestUtil(
    NextProto protocol) : spdy_util_(protocol) {}

std::string SpdyWebSocketTestUtil::GetHeader(const SpdyHeaderBlock& headers,
                                             const std::string& key) const {
  SpdyHeaderBlock::const_iterator it = headers.find(GetHeaderKey(key));
  return (it == headers.end()) ? "" : it->second;
}

void SpdyWebSocketTestUtil::SetHeader(
    const std::string& key,
    const std::string& value,
    SpdyHeaderBlock* headers) const {
  (*headers)[GetHeaderKey(key)] = value;
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdyWebSocketSynStream(
    int stream_id,
    const char* path,
    const char* host,
    const char* origin) {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  SetHeader("path", path, headers.get());
  SetHeader("host", host, headers.get());
  SetHeader("version", "WebSocket/13", headers.get());
  SetHeader("scheme", "ws", headers.get());
  SetHeader("origin", origin, headers.get());
  return spdy_util_.ConstructSpdyControlFrame(headers.Pass(),
                                              /*compressed*/ false,
                                              stream_id,
                                              HIGHEST,
                                              SYN_STREAM,
                                              CONTROL_FLAG_NONE,
                                              0);
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdyWebSocketSynReply(
    int stream_id) {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  SetHeader("status", "101", headers.get());
  return spdy_util_.ConstructSpdyControlFrame(headers.Pass(),
                                              false,
                                              stream_id,
                                              LOWEST,
                                              SYN_REPLY,
                                              CONTROL_FLAG_NONE,
                                              0);
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdyWebSocketHandshakeRequestFrame(
    scoped_ptr<SpdyHeaderBlock> headers,
    SpdyStreamId stream_id,
    RequestPriority request_priority) {
  // SPDY SYN_STREAM control frame header.
  const SpdyHeaderInfo kSynStreamHeader = {
    SYN_STREAM,
    stream_id,
    kDefaultAssociatedStreamId,
    ConvertRequestPriorityToSpdyPriority(request_priority, SPDY2),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,
    kDefaultCompressed,
    RST_STREAM_INVALID,
    kDefaultDataPointer,
    kDefaultDataLength,
    DATA_FLAG_NONE
  };

  // Construct SPDY SYN_STREAM control frame.
  return spdy_util_.ConstructSpdyFrame(
      kSynStreamHeader,
      headers.Pass());
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdyWebSocketHandshakeResponseFrame(
    scoped_ptr<SpdyHeaderBlock> headers,
    SpdyStreamId stream_id,
    RequestPriority request_priority) {
  // SPDY SYN_REPLY control frame header.
  const SpdyHeaderInfo kSynReplyHeader = {
    SYN_REPLY,
    stream_id,
    kDefaultAssociatedStreamId,
    ConvertRequestPriorityToSpdyPriority(request_priority, SPDY2),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,
    kDefaultCompressed,
    RST_STREAM_INVALID,
    kDefaultDataPointer,
    kDefaultDataLength,
    DATA_FLAG_NONE
  };

  // Construct SPDY SYN_REPLY control frame.
  return spdy_util_.ConstructSpdyFrame(
      kSynReplyHeader,
      headers.Pass());
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdyWebSocketHeadersFrame(
    int stream_id,
    const char* length,
    bool fin) {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  SetHeader("opcode", "1", headers.get());  // text frame
  SetHeader("length", length, headers.get());
  SetHeader("fin", fin ? "1" : "0", headers.get());
  return spdy_util_.ConstructSpdyControlFrame(headers.Pass(),
                                              /*compression*/ false,
                                              stream_id,
                                              LOWEST,
                                              HEADERS,
                                              CONTROL_FLAG_NONE,
                                              0);
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdyWebSocketDataFrame(
    const char* data,
    int len,
    SpdyStreamId stream_id,
    bool fin) {

  // Construct SPDY data frame.
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);
  return framer.CreateDataFrame(
      stream_id,
      data,
      len,
      fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdySettings(
    const SettingsMap& settings) const {
  return spdy_util_.ConstructSpdySettings(settings);
}

SpdyFrame* SpdyWebSocketTestUtil::ConstructSpdySettingsAck() const {
  return spdy_util_.ConstructSpdySettingsAck();
}

SpdyMajorVersion SpdyWebSocketTestUtil::spdy_version() const {
  return spdy_util_.spdy_version();
}

std::string SpdyWebSocketTestUtil::GetHeaderKey(
    const std::string& key) const {
  return (spdy_util_.is_spdy2() ? "" : ":") + key;
}

}  // namespace net
