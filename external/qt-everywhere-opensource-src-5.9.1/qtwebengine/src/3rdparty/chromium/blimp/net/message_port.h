// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_MESSAGE_PORT_H_
#define BLIMP_NET_MESSAGE_PORT_H_

#include <memory>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "blimp/net/blimp_net_export.h"

namespace net {
class StreamSocket;
}

namespace blimp {

class PacketReader;
class PacketWriter;

// A duplexed pair of a framed reader and writer.
class BLIMP_NET_EXPORT MessagePort {
 public:
  MessagePort(std::unique_ptr<PacketReader> reader,
              std::unique_ptr<PacketWriter> writer);
  virtual ~MessagePort();

  static std::unique_ptr<MessagePort> CreateForStreamSocketWithCompression(
      std::unique_ptr<net::StreamSocket> socket);

  PacketReader* reader() const { return reader_.get(); }
  PacketWriter* writer() const { return writer_.get(); }

 private:
  std::unique_ptr<PacketReader> reader_;
  std::unique_ptr<PacketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(MessagePort);
};

}  // namespace blimp

#endif  // BLIMP_NET_MESSAGE_PORT_H_
