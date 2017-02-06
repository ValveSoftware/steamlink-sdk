// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_COMPRESSED_PACKET_READER_H_
#define BLIMP_NET_COMPRESSED_PACKET_READER_H_

#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/packet_reader.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "third_party/zlib/zlib.h"

namespace blimp {

// Filter object which wraps a PacketReader, adding a DEFLATE decompression step
// to received packets.
// Details about the encoding format are available in the CompressedPacketWriter
// header.
class BLIMP_NET_EXPORT CompressedPacketReader : public PacketReader {
 public:
  // |source|: The source which from which compressed packets are read.
  explicit CompressedPacketReader(std::unique_ptr<PacketReader> source);

  ~CompressedPacketReader() override;

  // PacketReader implementation.
  void ReadPacket(const scoped_refptr<net::GrowableIOBuffer>& decompressed_buf,
                  const net::CompletionCallback& cb) override;

 private:
  // Called when source->ReadPacket() has completed.
  // |buf|: The buffer which will receive decompressed data.
  // |cb|: Callback which is triggered after decompression has finished.
  // |result|: The result of the read operation.
  void OnCompressedPacketReceived(
      const scoped_refptr<net::GrowableIOBuffer> decompressed_buf,
      const net::CompletionCallback& cb,
      int result);

  // Decompresses the contents of |compressed_buf_| to the buffer
  // |decompressed|.
  // |decompressed_size| is set to the size of the decompressed payload.
  // On success, returns a >= 0 value indicating the size of the
  // decompressed payload.
  // On failure, returns a negative value indicating the error code
  // (see net_errors.h).
  int DecompressPacket(const scoped_refptr<net::GrowableIOBuffer>& decompressed,
                       int size);

  std::unique_ptr<PacketReader> source_;
  scoped_refptr<net::GrowableIOBuffer> compressed_buf_;
  z_stream zlib_stream_;

  // Used to abandon pending ReadPacket() calls on teardown.
  base::WeakPtrFactory<CompressedPacketReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompressedPacketReader);
};

}  // namespace blimp

#endif  // BLIMP_NET_COMPRESSED_PACKET_READER_H_
