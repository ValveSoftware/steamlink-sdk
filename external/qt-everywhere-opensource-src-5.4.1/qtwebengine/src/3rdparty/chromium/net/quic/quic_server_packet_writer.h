// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SERVER_PACKET_WRITER_H_
#define NET_QUIC_QUIC_SERVER_PACKET_WRITER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/udp/udp_server_socket.h"

namespace net {

struct WriteResult;

// Chrome specific packet writer which uses a UDPServerSocket for writing
// data.
class QuicServerPacketWriter : public QuicPacketWriter {
 public:
  QuicServerPacketWriter();
  explicit QuicServerPacketWriter(UDPServerSocket* socket);
  virtual ~QuicServerPacketWriter();

  // QuicPacketWriter
  virtual WriteResult WritePacket(const char* buffer,
                                  size_t buf_len,
                                  const net::IPAddressNumber& self_address,
                                  const net::IPEndPoint& peer_address) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;
  virtual bool IsWriteBlocked() const OVERRIDE;
  virtual void SetWritable() OVERRIDE;

  void OnWriteComplete(int rv);
  void SetConnection(QuicConnection* connection) {
    connection_ = connection;
  }

 private:
  base::WeakPtrFactory<QuicServerPacketWriter> weak_factory_;
  UDPServerSocket* socket_;
  QuicConnection* connection_;
  bool write_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicServerPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SERVER_PACKET_WRITER_H_
