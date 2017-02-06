// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/stream_socket_connection.h"

#include "base/memory/ptr_util.h"
#include "blimp/net/compressed_packet_reader.h"
#include "blimp/net/compressed_packet_writer.h"
#include "blimp/net/stream_packet_reader.h"
#include "blimp/net/stream_packet_writer.h"

namespace blimp {

StreamSocketConnection::StreamSocketConnection(
    std::unique_ptr<net::StreamSocket> socket,
    BlimpConnectionStatistics* statistics)
    : BlimpConnection(
          base::WrapUnique(new CompressedPacketReader(base::WrapUnique(
              new StreamPacketReader(socket.get(), statistics)))),
          base::WrapUnique(new CompressedPacketWriter(base::WrapUnique(
              new StreamPacketWriter(socket.get(), statistics))))),
      socket_(std::move(socket)) {
  DCHECK(socket_);
  DCHECK(statistics);
}

StreamSocketConnection::~StreamSocketConnection() {}

}  // namespace blimp
