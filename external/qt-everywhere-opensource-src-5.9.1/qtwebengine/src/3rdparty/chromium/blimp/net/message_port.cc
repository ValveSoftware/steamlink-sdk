// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/message_port.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "blimp/net/compressed_packet_reader.h"
#include "blimp/net/compressed_packet_writer.h"
#include "blimp/net/packet_reader.h"
#include "blimp/net/packet_writer.h"
#include "blimp/net/stream_packet_reader.h"
#include "blimp/net/stream_packet_writer.h"
#include "net/socket/stream_socket.h"

namespace blimp {
namespace {

class CompressedStreamSocketMessagePort : public MessagePort {
 public:
  explicit CompressedStreamSocketMessagePort(
      std::unique_ptr<net::StreamSocket> socket);
  ~CompressedStreamSocketMessagePort() override {}

 private:
  std::unique_ptr<net::StreamSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(CompressedStreamSocketMessagePort);
};

CompressedStreamSocketMessagePort::CompressedStreamSocketMessagePort(
    std::unique_ptr<net::StreamSocket> socket)
    : MessagePort(base::MakeUnique<CompressedPacketReader>(
                      base::MakeUnique<StreamPacketReader>(socket.get())),
                  base::MakeUnique<CompressedPacketWriter>(
                      base::MakeUnique<StreamPacketWriter>(socket.get()))),
      socket_(std::move(socket)) {}

}  // namespace

// static
std::unique_ptr<MessagePort> MessagePort::CreateForStreamSocketWithCompression(
    std::unique_ptr<net::StreamSocket> socket) {
  return base::MakeUnique<CompressedStreamSocketMessagePort>(std::move(socket));
}

MessagePort::MessagePort(std::unique_ptr<PacketReader> reader,
                         std::unique_ptr<PacketWriter> writer)
    : reader_(std::move(reader)), writer_(std::move(writer)) {}

MessagePort::~MessagePort() {}

}  // namespace blimp
