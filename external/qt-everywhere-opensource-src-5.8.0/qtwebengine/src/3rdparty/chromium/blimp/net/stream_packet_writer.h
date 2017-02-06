// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_STREAM_PACKET_WRITER_H_
#define BLIMP_NET_STREAM_PACKET_WRITER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/packet_writer.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

namespace net {
class DrainableIOBuffer;
class StreamSocket;
}  // namespace net

namespace blimp {
class BlimpConnectionStatistics;

// Writes opaque length-prefixed packets to a StreamSocket.
// The header segment is 32-bit, encoded in network byte order.
// The body segment length is specified in the header (should be capped at
//     kMaxPacketPayloadSizeBytes).
class BLIMP_NET_EXPORT StreamPacketWriter : public PacketWriter {
 public:
  // |socket|: The socket to write packets to. The caller must ensure |socket|
  // is valid while the reader is in-use (see ReadPacket below).
  // |statistics|: Statistics collector which keeps track of number of bytes
  // written. |statistics| is expected to outlive |this|.
  StreamPacketWriter(net::StreamSocket* socket,
                     BlimpConnectionStatistics* statistics);

  ~StreamPacketWriter() override;

  // PacketWriter implementation.
  void WritePacket(const scoped_refptr<net::DrainableIOBuffer>& data,
                   const net::CompletionCallback& callback) override;

 private:
  enum class WriteState {
    IDLE,
    HEADER,
    PAYLOAD,
  };

  friend std::ostream& operator<<(std::ostream& out, const WriteState state);

  // State machine implementation.
  // |result| - the result value of the most recent network operation.
  // See comments for WritePacket() for documentation on return values.
  int DoWriteLoop(int result);

  int DoWriteHeader(int result);

  int DoWritePayload(int result);

  // Callback function to be invoked on asynchronous write completion.
  // Invokes |callback_| on packet write completion or on error.
  void OnWriteComplete(int result);

  // Executes a socket write.
  // Returns a positive value indicating the number of bytes written
  // on success.
  // Returns a negative net::Error value if the socket was closed or an error
  // occurred.
  int DoWrite(net::IOBuffer* buf, int buf_len);

  WriteState write_state_;

  net::StreamSocket* socket_;

  scoped_refptr<net::DrainableIOBuffer> payload_buffer_;
  scoped_refptr<net::DrainableIOBuffer> header_buffer_;
  net::CompletionCallback callback_;
  BlimpConnectionStatistics* statistics_;

  base::WeakPtrFactory<StreamPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamPacketWriter);
};

}  // namespace blimp

#endif  // BLIMP_NET_STREAM_PACKET_WRITER_H_
