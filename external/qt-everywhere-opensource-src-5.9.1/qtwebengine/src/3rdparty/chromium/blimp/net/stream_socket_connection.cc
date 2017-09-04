// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/stream_socket_connection.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "blimp/net/compressed_packet_reader.h"
#include "blimp/net/compressed_packet_writer.h"
#include "blimp/net/stream_packet_reader.h"
#include "blimp/net/stream_packet_writer.h"

namespace blimp {

StreamSocketConnection::StreamSocketConnection(
    std::unique_ptr<net::StreamSocket> socket)
    : BlimpConnection(base::MakeUnique<CompressedPacketReader>(
                          base::MakeUnique<StreamPacketReader>(socket.get())),
                      base::MakeUnique<CompressedPacketWriter>(
                          base::MakeUnique<StreamPacketWriter>(socket.get()))),
      socket_(std::move(socket)) {
  DCHECK(socket_);
}

StreamSocketConnection::~StreamSocketConnection() {}

}  // namespace blimp
