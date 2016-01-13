// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_
#define NET_TOOLS_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_packet_writer.h"

namespace net {

struct WriteResult;

namespace tools {

// Default packet writer which wraps QuicSocketUtils WritePacket.
class QuicDefaultPacketWriter : public QuicPacketWriter {
 public:
  explicit QuicDefaultPacketWriter(int fd);
  virtual ~QuicDefaultPacketWriter();

  // QuicPacketWriter
  virtual WriteResult WritePacket(const char* buffer,
                                  size_t buf_len,
                                  const IPAddressNumber& self_address,
                                  const IPEndPoint& peer_address) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;
  virtual bool IsWriteBlocked() const OVERRIDE;
  virtual void SetWritable() OVERRIDE;

  void set_fd(int fd) { fd_ = fd; }

 protected:
  void set_write_blocked(bool is_blocked) {
    write_blocked_ = is_blocked;
  }
  int fd() { return fd_; }

 private:
  int fd_;
  bool write_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicDefaultPacketWriter);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_DEFAULT_PACKET_WRITER_H_
