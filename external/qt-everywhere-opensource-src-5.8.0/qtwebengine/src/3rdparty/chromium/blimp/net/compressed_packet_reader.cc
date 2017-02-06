// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/compressed_packet_reader.h"

#include <iostream>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_byteorder.h"
#include "blimp/net/common.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace blimp {

CompressedPacketReader::CompressedPacketReader(
    std::unique_ptr<PacketReader> source)
    : source_(std::move(source)),
      compressed_buf_(new net::GrowableIOBuffer),
      weak_factory_(this) {
  DCHECK(source_);

  memset(&zlib_stream_, 0, sizeof(z_stream));

  // MAX_WBITS means we are using the maximal window size for decompression;
  // a negative value means that we are ignoring headers and CRC checks.
  int init_result = inflateInit2(&zlib_stream_, -MAX_WBITS);
  DCHECK_EQ(Z_OK, init_result);
}

CompressedPacketReader::~CompressedPacketReader() {
  inflateEnd(&zlib_stream_);
}

void CompressedPacketReader::ReadPacket(
    const scoped_refptr<net::GrowableIOBuffer>& decompressed_buf,
    const net::CompletionCallback& callback) {
  DCHECK(decompressed_buf);
  DCHECK(!callback.is_null());
  source_->ReadPacket(
      compressed_buf_,
      base::Bind(&CompressedPacketReader::OnCompressedPacketReceived,
                 weak_factory_.GetWeakPtr(), decompressed_buf, callback));
}

void CompressedPacketReader::OnCompressedPacketReceived(
    const scoped_refptr<net::GrowableIOBuffer> decompressed_buf,
    const net::CompletionCallback& callback,
    int result) {
  if (result <= 0) {
    callback.Run(result);
    return;
  }

  callback.Run(DecompressPacket(decompressed_buf, result));
}

int CompressedPacketReader::DecompressPacket(
    const scoped_refptr<net::GrowableIOBuffer>& decompressed,
    int size) {
  compressed_buf_->set_offset(0);
  decompressed->set_offset(0);
  if (static_cast<size_t>(decompressed->capacity()) <
      kMaxPacketPayloadSizeBytes) {
    decompressed->SetCapacity(kMaxPacketPayloadSizeBytes);
  }

  zlib_stream_.next_in = reinterpret_cast<uint8_t*>(compressed_buf_->data());
  zlib_stream_.avail_in = base::checked_cast<uint32_t>(size);
  zlib_stream_.next_out = reinterpret_cast<uint8_t*>(decompressed->data());
  zlib_stream_.avail_out = decompressed->RemainingCapacity();
  int inflate_result = inflate(&zlib_stream_, Z_SYNC_FLUSH);
  if (inflate_result != Z_OK) {
    DLOG(ERROR) << "inflate() returned unexpected error code: "
                << inflate_result;
    return net::ERR_UNEXPECTED;
  }
  DCHECK_GT(decompressed->RemainingCapacity(),
            base::checked_cast<int>(zlib_stream_.avail_out));
  int decompressed_size =
      decompressed->RemainingCapacity() - zlib_stream_.avail_out;

  // Verify that the decompressed output isn't bigger than the maximum allowable
  // payload size, by checking if there are bytes waiting to be processed.
  if (zlib_stream_.avail_in > 0) {
    DLOG(ERROR)
        << "Decompressed buffer size exceeds allowable limits; aborting.";
    return net::ERR_FILE_TOO_BIG;
  }

  return decompressed_size;
}

}  // namespace blimp
