// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_server_packet_writer.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

QuicServerPacketWriter::QuicServerPacketWriter() : weak_factory_(this) {
}

QuicServerPacketWriter::QuicServerPacketWriter(UDPServerSocket* socket)
    : weak_factory_(this),
      socket_(socket),
      write_blocked_(false) {
}

QuicServerPacketWriter::~QuicServerPacketWriter() {
}

WriteResult QuicServerPacketWriter::WritePacket(
    const char* buffer, size_t buf_len,
    const net::IPAddressNumber& self_address,
    const net::IPEndPoint& peer_address) {
  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(buffer, buf_len)));
  DCHECK(!IsWriteBlocked());
  int rv = socket_->SendTo(buf.get(),
                           buf_len,
                           peer_address,
                           base::Bind(&QuicServerPacketWriter::OnWriteComplete,
                                      weak_factory_.GetWeakPtr()));
  WriteStatus status = WRITE_STATUS_OK;
  if (rv < 0) {
    if (rv != ERR_IO_PENDING) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.WriteError", -rv);
      status = WRITE_STATUS_ERROR;
    } else {
      status = WRITE_STATUS_BLOCKED;
      write_blocked_ = true;
    }
  }

  return WriteResult(status, rv);
}

bool QuicServerPacketWriter::IsWriteBlockedDataBuffered() const {
  // UDPServerSocket::SendTo buffers the data until the Write is permitted.
  return true;
}

bool QuicServerPacketWriter::IsWriteBlocked() const {
  return write_blocked_;
}

void QuicServerPacketWriter::SetWritable() {
  write_blocked_ = false;
}

void QuicServerPacketWriter::OnWriteComplete(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  write_blocked_ = false;
  WriteResult result(rv < 0 ? WRITE_STATUS_ERROR : WRITE_STATUS_OK, rv);
  connection_->OnPacketSent(result);
  connection_->OnCanWrite();
}

}  // namespace net
