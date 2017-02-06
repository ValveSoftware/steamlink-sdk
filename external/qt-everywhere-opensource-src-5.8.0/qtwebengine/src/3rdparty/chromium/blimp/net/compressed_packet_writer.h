// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_COMPRESSED_PACKET_WRITER_H_
#define BLIMP_NET_COMPRESSED_PACKET_WRITER_H_

#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/packet_writer.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "third_party/zlib/zlib.h"

namespace blimp {

// Filter object which wraps a PacketWriter, adding a DEFLATE compression step
// to outgoing packets.
//
// Each packet is appended as a DEFLATE block to a long-lived compression
// stream.
// Blocks are explicitly flushed at each packet boundary, which prevents zlib's
// internal buffers from introducing unwanted output latency.
// Gzip headers and CRC checks are omitted from the stream output.
class BLIMP_NET_EXPORT CompressedPacketWriter : public PacketWriter {
 public:
  // |source|: The PacketWriter which will receive compressed packets.
  explicit CompressedPacketWriter(std::unique_ptr<PacketWriter> sink);
  ~CompressedPacketWriter() override;

  // PacketWriter implementation.
  void WritePacket(const scoped_refptr<net::DrainableIOBuffer>& buf,
                   const net::CompletionCallback& cb) override;

 private:
  // Compresses |src_buf| into |dest_buf|.
  // On success, returns a >= 0 value indicating the length of the
  // compressed payload.
  // On failure, returns a negative value indicating the error code
  // (see net_errors.h).
  int Compress(const scoped_refptr<net::DrainableIOBuffer>& src_buf,
               const scoped_refptr<net::GrowableIOBuffer>& dest_buf);

  z_stream zlib_stream_;
  std::unique_ptr<PacketWriter> sink_;
  scoped_refptr<net::GrowableIOBuffer> compressed_buf_;
  size_t uncompressed_size_total_ = 0u;
  size_t compressed_size_total_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(CompressedPacketWriter);
};

}  // namespace blimp

#endif  // BLIMP_NET_COMPRESSED_PACKET_WRITER_H_
