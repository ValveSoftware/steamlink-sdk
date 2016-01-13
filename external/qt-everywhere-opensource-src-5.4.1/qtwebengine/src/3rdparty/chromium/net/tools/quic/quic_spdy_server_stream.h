// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SPDY_SERVER_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_SPDY_SERVER_STREAM_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/io_buffer.h"
#include "net/quic/quic_data_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/balsa/balsa_headers.h"

namespace net {

class QuicSession;

namespace tools {

namespace test {
class QuicSpdyServerStreamPeer;
}  // namespace test

// All this does right now is aggregate data, and on fin, send an HTTP
// response.
class QuicSpdyServerStream : public QuicDataStream {
 public:
  QuicSpdyServerStream(QuicStreamId id, QuicSession* session);
  virtual ~QuicSpdyServerStream();

  // ReliableQuicStream implementation called by the session when there's
  // data for us.
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE;
  virtual void OnFinRead() OVERRIDE;

  int ParseRequestHeaders();

 private:
  friend class test::QuicSpdyServerStreamPeer;

  // Sends a basic 200 response using SendHeaders for the headers and WriteData
  // for the body.
  void SendResponse();

  // Sends a basic 500 response using SendHeaders for the headers and WriteData
  // for the body
  void SendErrorResponse();

  void SendHeadersAndBody(const BalsaHeaders& response_headers,
                          base::StringPiece body);

  BalsaHeaders headers_;
  string body_;

  // Buffer into which response header data is read.
  scoped_refptr<GrowableIOBuffer> read_buf_;
  bool request_headers_received_;

  DISALLOW_COPY_AND_ASSIGN(QuicSpdyServerStream);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SPDY_SERVER_STREAM_H_
