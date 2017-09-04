// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/compressed_packet_writer.h"

#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/sys_byteorder.h"
#include "blimp/net/common.h"
#include "net/base/io_buffer.h"
#include "third_party/zlib/zlib.h"

namespace blimp {
namespace {

// Allocate the maxmimum amount of memory to deflate (512KB) for higher
// compression. (See zconf.h for details on memLevel semantics.)
const int kZlibMemoryLevel = 9;

// Byte count of the zlib end-of-block marker (0x00, 0x00, 0xFF, 0xFF).
// Used for buffer preallocation.
const size_t kZlibBlockMarkerSize = 4;

}  // namespace

CompressedPacketWriter::CompressedPacketWriter(
    std::unique_ptr<PacketWriter> sink)
    : sink_(std::move(sink)), compressed_buf_(new net::GrowableIOBuffer) {
  DCHECK(sink_);

  memset(&zlib_stream_, 0, sizeof(z_stream));

  // MAX_WBITS means we are using the maximal window size for decompression;
  // negating it means that we are ignoring headers and CRC checks.
  int init_error =
      deflateInit2(&zlib_stream_, Z_BEST_COMPRESSION, Z_DEFLATED,
                   -MAX_WBITS,  // Negative value = no headers or CRC.
                   kZlibMemoryLevel, Z_DEFAULT_STRATEGY);
  CHECK_EQ(Z_OK, init_error);
}

CompressedPacketWriter::~CompressedPacketWriter() {
  deflateEnd(&zlib_stream_);
}

void CompressedPacketWriter::WritePacket(
    const scoped_refptr<net::DrainableIOBuffer>& write_buffer,
    const net::CompletionCallback& callback) {
  DCHECK(write_buffer);
  DCHECK(!callback.is_null());
  size_t uncompressed_size =
      base::checked_cast<size_t>(write_buffer->BytesRemaining());

  // Zero-length input => zero-length output.
  if (uncompressed_size == 0) {
    sink_->WritePacket(write_buffer, callback);
    return;
  }

  // Don't compress anything that's bigger than the maximum allowable payload
  // size.
  if (uncompressed_size > kMaxPacketPayloadSizeBytes) {
    callback.Run(net::ERR_FILE_TOO_BIG);
    return;
  }

  int compress_result = Compress(write_buffer, compressed_buf_);
  if (compress_result < 0) {
    callback.Run(compress_result);
    return;
  }

#if !defined(NDEBUG)
  uncompressed_size_total_ += uncompressed_size;
  compressed_size_total_ += compress_result;
#endif  // !defined(NDEBUG)

  scoped_refptr<net::DrainableIOBuffer> compressed_outbuf(
      new net::DrainableIOBuffer(compressed_buf_.get(), compress_result));
  sink_->WritePacket(compressed_outbuf, callback);
  DVLOG(4) << "deflate packet: " << uncompressed_size << " in, "
           << compress_result << " out.";
  DVLOG(3) << "deflate total: " << uncompressed_size_total_ << " in, "
           << compressed_size_total_ << " out.";
}

int CompressedPacketWriter::Compress(
    const scoped_refptr<net::DrainableIOBuffer>& src_buf,
    const scoped_refptr<net::GrowableIOBuffer>& dest_buf) {
  DCHECK_EQ(0, dest_buf->offset());

  // Preallocate a buffer that's large enough to fit the compressed contents of
  // src_buf, plus some overhead for zlib.
  const int zlib_output_ubound =
      deflateBound(&zlib_stream_, src_buf->BytesRemaining()) +
      kZlibBlockMarkerSize;
  if (dest_buf->capacity() < zlib_output_ubound) {
    dest_buf->SetCapacity(zlib_output_ubound);
  }

  zlib_stream_.next_in = reinterpret_cast<uint8_t*>(src_buf->data());
  zlib_stream_.avail_in = static_cast<unsigned>(src_buf->BytesRemaining());
  zlib_stream_.next_out = reinterpret_cast<uint8_t*>(dest_buf->data());
  zlib_stream_.avail_out = static_cast<unsigned>(zlib_output_ubound);
  int deflate_error = deflate(&zlib_stream_, Z_SYNC_FLUSH);

  if (deflate_error != Z_OK) {
    DLOG(FATAL) << "Unexpected deflate() return value: " << deflate_error;
    return net::ERR_UNEXPECTED;
  }
  if (zlib_stream_.avail_in > 0) {
    DLOG(ERROR) << "deflate() did not consume all data, remainder: "
                << zlib_stream_.avail_in << " bytes.";
    return net::ERR_UNEXPECTED;
  }

  return zlib_output_ubound - zlib_stream_.avail_out;
}

}  // namespace blimp
