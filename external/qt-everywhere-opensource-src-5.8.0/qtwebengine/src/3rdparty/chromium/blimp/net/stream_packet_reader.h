// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_STREAM_PACKET_READER_H_
#define BLIMP_NET_STREAM_PACKET_READER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/packet_reader.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

namespace net {
class GrowableIOBuffer;
class StreamSocket;
}  // namespace net

namespace blimp {
class BlimpConnectionStatistics;

// Reads opaque length-prefixed packets of bytes from a StreamSocket.
// The header segment is 32-bit, encoded in network byte order.
// The body segment length is specified in the header (capped at
//     kMaxPacketPayloadSizeBytes).
class BLIMP_NET_EXPORT StreamPacketReader : public PacketReader {
 public:
  // |socket|: The socket to read packets from. The caller must ensure |socket|
  // is valid while the reader is in-use (see ReadPacket below).
  // |statistics|: Statistics collector to keep track of number of bytes read.
  // |statistics| is expected to outlive |this|.
  StreamPacketReader(net::StreamSocket* socket,
                     BlimpConnectionStatistics* statistics);

  ~StreamPacketReader() override;

  // PacketReader implementation.
  void ReadPacket(const scoped_refptr<net::GrowableIOBuffer>& buf,
                  const net::CompletionCallback& cb) override;

 private:
  enum class ReadState {
    IDLE,
    HEADER,
    PAYLOAD,
  };

  friend std::ostream& operator<<(std::ostream& out, const ReadState state);

  // State machine implementation.
  // |result| - the result value of the most recent network operation.
  // See comments for ReadPacket() for documentation on return values.
  int DoReadLoop(int result);

  // Reads the header and parses it when, done, to get the payload size.
  int DoReadHeader(int result);

  // Reads payload bytes until the payload is complete.
  int DoReadPayload(int result);

  // Executes a socket read.
  // Returns a positive value indicating the number of bytes read on success.
  // Returns a negative net::Error value if the socket was closed or an error
  // occurred.
  int DoRead(net::IOBuffer* buf, int buf_len);

  // Processes an asynchronous header or payload read, and invokes |callback_|
  // on packet read completion.
  void OnReadComplete(int result);

  ReadState read_state_;

  net::StreamSocket* socket_;

  // The size of the payload, in bytes.
  size_t payload_size_;

  scoped_refptr<net::GrowableIOBuffer> header_buffer_;
  scoped_refptr<net::GrowableIOBuffer> payload_buffer_;
  net::CompletionCallback callback_;
  BlimpConnectionStatistics* statistics_;

  base::WeakPtrFactory<StreamPacketReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamPacketReader);
};

}  // namespace blimp

#endif  // BLIMP_NET_STREAM_PACKET_READER_H_
