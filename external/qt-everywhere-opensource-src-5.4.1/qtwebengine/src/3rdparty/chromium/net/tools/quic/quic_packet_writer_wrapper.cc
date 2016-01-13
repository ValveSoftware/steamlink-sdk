// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_packet_writer_wrapper.h"

#include "net/quic/quic_types.h"

namespace net {
namespace tools {

QuicPacketWriterWrapper::QuicPacketWriterWrapper() {}

QuicPacketWriterWrapper::QuicPacketWriterWrapper(QuicPacketWriter* writer)
    : writer_(writer) {}

QuicPacketWriterWrapper::~QuicPacketWriterWrapper() {}

WriteResult QuicPacketWriterWrapper::WritePacket(
    const char* buffer,
    size_t buf_len,
    const net::IPAddressNumber& self_address,
    const net::IPEndPoint& peer_address) {
  return writer_->WritePacket(buffer, buf_len, self_address, peer_address);
}

bool QuicPacketWriterWrapper::IsWriteBlockedDataBuffered() const {
  return writer_->IsWriteBlockedDataBuffered();
}

bool QuicPacketWriterWrapper::IsWriteBlocked() const {
  return writer_->IsWriteBlocked();
}

void QuicPacketWriterWrapper::SetWritable() {
  writer_->SetWritable();
}

void QuicPacketWriterWrapper::set_writer(QuicPacketWriter* writer) {
  writer_.reset(writer);
}

QuicPacketWriter* QuicPacketWriterWrapper::release_writer() {
  return writer_.release();
}

}  // namespace tools
}  // namespace net
