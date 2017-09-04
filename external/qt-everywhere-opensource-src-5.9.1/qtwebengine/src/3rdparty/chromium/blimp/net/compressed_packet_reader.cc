// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/compressed_packet_reader.h"

#include <algorithm>
#include <iostream>
#include <utility>

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
namespace {

constexpr double kInitialDecompressionBufferSizeFactor = 1.5;
constexpr double kDecompressionGrowthFactor = 2.0;

}  // namespace

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
    const scoped_refptr<net::GrowableIOBuffer>& decompressed_buf,
    int size_compressed) {
  scoped_refptr<net::DrainableIOBuffer> drainable_input(
      new net::DrainableIOBuffer(compressed_buf_.get(), size_compressed));

  // Prepare the sink for decompressed data.
  decompressed_buf->set_offset(0);
  const int min_size = kInitialDecompressionBufferSizeFactor * size_compressed;
  if (decompressed_buf->capacity() < min_size) {
    decompressed_buf->SetCapacity(min_size);
  }

  // Repeatedly decompress |drainable_input| until it's fully consumed, growing
  // |decompressed_buf| as necessary to accomodate the decompressed output.
  do {
    zlib_stream_.next_in = reinterpret_cast<uint8_t*>(drainable_input->data());
    zlib_stream_.avail_in = drainable_input->BytesRemaining();
    zlib_stream_.next_out =
        reinterpret_cast<uint8_t*>(decompressed_buf->data());
    zlib_stream_.avail_out = decompressed_buf->RemainingCapacity();
    int inflate_result = inflate(&zlib_stream_, Z_SYNC_FLUSH);
    if (inflate_result != Z_OK) {
      DLOG(ERROR) << "inflate() returned unexpected error code: "
                  << inflate_result;
      return net::ERR_UNEXPECTED;
    }

    // Process the inflate() result.
    const int bytes_in =
        drainable_input->BytesRemaining() - zlib_stream_.avail_in;
    const int bytes_out =
        (decompressed_buf->RemainingCapacity() - zlib_stream_.avail_out);
    drainable_input->DidConsume(bytes_in);
    decompressed_buf->set_offset(decompressed_buf->offset() + bytes_out);
    if (static_cast<size_t>(decompressed_buf->offset()) >
        kMaxPacketPayloadSizeBytes) {
      DLOG(ERROR)
          << "Decompressed buffer size exceeds allowable limits; aborting.";
      return net::ERR_FILE_TOO_BIG;
    }

    if (drainable_input->BytesRemaining() > 0) {
      // Output buffer isn't large enough to fit the compressed input, so
      // enlarge it.
      DCHECK_GT(zlib_stream_.avail_in, 0u);
      DCHECK_EQ(0u, zlib_stream_.avail_out);

      decompressed_buf->SetCapacity(
          std::min(static_cast<size_t>(kDecompressionGrowthFactor *
                                       decompressed_buf->capacity()),
                   kMaxPacketPayloadSizeBytes + 1));
      VLOG(2) << "Increase buffer size to " << decompressed_buf->capacity()
              << " bytes.";
    }
  } while (zlib_stream_.avail_in > 0);

  int total_decompressed_size = decompressed_buf->offset();
  decompressed_buf->set_offset(0);
  return total_decompressed_size;
}

}  // namespace blimp
