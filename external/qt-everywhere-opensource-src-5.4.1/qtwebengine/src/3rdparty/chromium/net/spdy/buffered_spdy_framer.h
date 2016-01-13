// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#define NET_SPDY_BUFFERED_SPDY_FRAMER_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

// Returns the SPDY major version corresponding to the given NextProto
// value, which must represent a SPDY-like protocol.
NET_EXPORT_PRIVATE SpdyMajorVersion NextProtoToSpdyMajorVersion(
    NextProto next_proto);

class NET_EXPORT_PRIVATE BufferedSpdyFramerVisitorInterface {
 public:
  BufferedSpdyFramerVisitorInterface() {}

  // Called if an error is detected in the SpdyFrame protocol.
  virtual void OnError(SpdyFramer::SpdyError error_code) = 0;

  // Called if an error is detected in a SPDY stream.
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) = 0;

  // Called after all the header data for SYN_STREAM control frame is received.
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) = 0;

  // Called after all the header data for SYN_REPLY control frame is received.
  virtual void OnSynReply(SpdyStreamId stream_id,
                          bool fin,
                          const SpdyHeaderBlock& headers) = 0;

  // Called after all the header data for HEADERS control frame is received.
  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool fin,
                         const SpdyHeaderBlock& headers) = 0;

  // Called when a data frame header is received.
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer (at most 2^24 - 1 for SPDY/3,
  // but 2^16 - 1 - 8 for SPDY/4).
  // When the other side has finished sending data on this stream,
  // this method will be called with a zero-length buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) = 0;

  // Called when a SETTINGS frame is received.
  // |clear_persisted| True if the respective flag is set on the SETTINGS frame.
  virtual void OnSettings(bool clear_persisted) = 0;

  // Called when an individual setting within a SETTINGS frame has been parsed
  // and validated.
  virtual void OnSetting(SpdySettingsIds id, uint8 flags, uint32 value) = 0;

  // Called when a SETTINGS frame is received with the ACK flag set.
  virtual void OnSettingsAck() {}

  // Called at the completion of parsing SETTINGS id and value tuples.
  virtual void OnSettingsEnd() {};

  // Called when a PING frame has been parsed.
  virtual void OnPing(SpdyPingId unique_id, bool is_ack) = 0;

  // Called when a RST_STREAM frame has been parsed.
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) = 0;

  // Called when a GOAWAY frame has been parsed.
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) = 0;

  // Called when a WINDOW_UPDATE frame has been parsed.
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) = 0;

  // Called when a PUSH_PROMISE frame has been parsed.
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             const SpdyHeaderBlock& headers) = 0;

 protected:
  virtual ~BufferedSpdyFramerVisitorInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramerVisitorInterface);
};

class NET_EXPORT_PRIVATE BufferedSpdyFramer
    : public SpdyFramerVisitorInterface {
 public:
  BufferedSpdyFramer(SpdyMajorVersion version,
                     bool enable_compression);
  virtual ~BufferedSpdyFramer();

  // Sets callbacks to be called from the buffered spdy framer.  A visitor must
  // be set, or else the framer will likely crash.  It is acceptable for the
  // visitor to do nothing.  If this is called multiple times, only the last
  // visitor will be used.
  void set_visitor(BufferedSpdyFramerVisitorInterface* visitor);

  // Set debug callbacks to be called from the framer. The debug visitor is
  // completely optional and need not be set in order for normal operation.
  // If this is called multiple times, only the last visitor will be used.
  void set_debug_visitor(SpdyFramerDebugVisitorInterface* debug_visitor);

  // SpdyFramerVisitorInterface
  virtual void OnError(SpdyFramer* spdy_framer) OVERRIDE;
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional) OVERRIDE;
  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE;
  virtual void OnHeaders(SpdyStreamId stream_id, bool fin, bool end) OVERRIDE;
  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE;
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE;
  virtual void OnSettings(bool clear_persisted) OVERRIDE;
  virtual void OnSetting(
      SpdySettingsIds id, uint8 flags, uint32 value) OVERRIDE;
  virtual void OnSettingsAck() OVERRIDE;
  virtual void OnSettingsEnd() OVERRIDE;
  virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE;
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE;
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE;
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE;
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             bool end) OVERRIDE;
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE;
  virtual void OnContinuation(SpdyStreamId stream_id, bool end) OVERRIDE;

  // SpdyFramer methods.
  size_t ProcessInput(const char* data, size_t len);
  SpdyMajorVersion protocol_version();
  void Reset();
  SpdyFramer::SpdyError error_code() const;
  SpdyFramer::SpdyState state() const;
  bool MessageFullyRead();
  bool HasError();
  SpdyFrame* CreateSynStream(SpdyStreamId stream_id,
                             SpdyStreamId associated_stream_id,
                             SpdyPriority priority,
                             SpdyControlFlags flags,
                             const SpdyHeaderBlock* headers);
  SpdyFrame* CreateSynReply(SpdyStreamId stream_id,
                            SpdyControlFlags flags,
                            const SpdyHeaderBlock* headers);
  SpdyFrame* CreateRstStream(SpdyStreamId stream_id,
                             SpdyRstStreamStatus status) const;
  SpdyFrame* CreateSettings(const SettingsMap& values) const;
  SpdyFrame* CreatePingFrame(uint32 unique_id, bool is_ack) const;
  SpdyFrame* CreateGoAway(
      SpdyStreamId last_accepted_stream_id,
      SpdyGoAwayStatus status) const;
  SpdyFrame* CreateHeaders(SpdyStreamId stream_id,
                           SpdyControlFlags flags,
                           const SpdyHeaderBlock* headers);
  SpdyFrame* CreateWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size) const;
  SpdyFrame* CreateDataFrame(SpdyStreamId stream_id,
                             const char* data,
                             uint32 len,
                             SpdyDataFlags flags);
  SpdyFrame* CreatePushPromise(SpdyStreamId stream_id,
                               SpdyStreamId promised_stream_id,
                               const SpdyHeaderBlock* headers);

  // Serialize a frame of unknown type.
  SpdySerializedFrame* SerializeFrame(const SpdyFrameIR& frame) {
    return spdy_framer_.SerializeFrame(frame);
  }

  SpdyPriority GetHighestPriority() const;

  size_t GetDataFrameMinimumSize() const {
    return spdy_framer_.GetDataFrameMinimumSize();
  }

  size_t GetControlFrameHeaderSize() const {
    return spdy_framer_.GetControlFrameHeaderSize();
  }

  size_t GetSynStreamMinimumSize() const {
    return spdy_framer_.GetSynStreamMinimumSize();
  }

  size_t GetFrameMinimumSize() const {
    return spdy_framer_.GetFrameMinimumSize();
  }

  size_t GetFrameMaximumSize() const {
    return spdy_framer_.GetFrameMaximumSize();
  }

  size_t GetDataFrameMaximumPayload() const {
    return spdy_framer_.GetDataFrameMaximumPayload();
  }

  int frames_received() const { return frames_received_; }

 private:
  // The size of the header_buffer_.
  enum { kHeaderBufferSize = 32 * 1024 };

  void InitHeaderStreaming(SpdyStreamId stream_id);

  SpdyFramer spdy_framer_;
  BufferedSpdyFramerVisitorInterface* visitor_;

  // Header block streaming state:
  char header_buffer_[kHeaderBufferSize];
  size_t header_buffer_used_;
  bool header_buffer_valid_;
  SpdyStreamId header_stream_id_;
  int frames_received_;

  // Collection of fields from control frames that we need to
  // buffer up from the spdy framer.
  struct ControlFrameFields {
    SpdyFrameType type;
    SpdyStreamId stream_id;
    SpdyStreamId associated_stream_id;
    SpdyStreamId promised_stream_id;
    SpdyPriority priority;
    uint8 credential_slot;
    bool fin;
    bool unidirectional;
  };
  scoped_ptr<ControlFrameFields> control_frame_fields_;

  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramer);
};

}  // namespace net

#endif  // NET_SPDY_BUFFERED_SPDY_FRAMER_H_
