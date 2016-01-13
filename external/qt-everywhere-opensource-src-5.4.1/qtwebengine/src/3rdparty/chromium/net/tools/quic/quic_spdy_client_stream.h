// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SPDY_CLIENT_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_SPDY_CLIENT_STREAM_H_

#include <sys/types.h>
#include <string>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/quic/quic_data_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/balsa/balsa_frame.h"
#include "net/tools/balsa/balsa_headers.h"

namespace net {

namespace tools {

class QuicClientSession;

// All this does right now is send an SPDY request, and aggregate the
// SPDY response.
class QuicSpdyClientStream : public QuicDataStream {
 public:
  QuicSpdyClientStream(QuicStreamId id, QuicClientSession* session);
  virtual ~QuicSpdyClientStream();

  // Override the base class to close the write side as soon as we get a
  // response.
  // SPDY/HTTP does not support bidirectional streaming.
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;

  // Override the base class to store the size of the headers.
  virtual void OnStreamHeadersComplete(bool fin, size_t frame_len) OVERRIDE;

  // ReliableQuicStream implementation called by the session when there's
  // data for us.
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE;

  virtual void OnFinRead() OVERRIDE;

  // Serializes the headers and body, sends it to the server, and
  // returns the number of bytes sent.
  ssize_t SendRequest(const BalsaHeaders& headers,
                      base::StringPiece body,
                      bool fin);

  // Sends body data to the server, or buffers if it can't be sent immediately.
  void SendBody(const std::string& data, bool fin);

  // Returns the response data.
  const std::string& data() { return data_; }

  // Returns whatever headers have been received for this stream.
  const BalsaHeaders& headers() { return headers_; }

  size_t header_bytes_read() const { return header_bytes_read_; }

  size_t header_bytes_written() const { return header_bytes_written_; }

  // While the server's set_priority shouldn't be called externally, the creator
  // of client-side streams should be able to set the priority.
  using QuicDataStream::set_priority;

 private:
  int ParseResponseHeaders();

  BalsaHeaders headers_;
  std::string data_;

  scoped_refptr<GrowableIOBuffer> read_buf_;
  bool response_headers_received_;
  size_t header_bytes_read_;
  size_t header_bytes_written_;

  DISALLOW_COPY_AND_ASSIGN(QuicSpdyClientStream);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SPDY_CLIENT_STREAM_H_
