// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_STREAM_SOCKET_CONNECTION_H_
#define BLIMP_NET_STREAM_SOCKET_CONNECTION_H_

#include <memory>

#include "blimp/net/blimp_connection.h"
#include "net/socket/stream_socket.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace blimp {
class BlimpConnectionStatistics;

// BlimpConnection specialization for StreamSocket-based connections.
class StreamSocketConnection : public BlimpConnection {
 public:
  StreamSocketConnection(std::unique_ptr<net::StreamSocket> socket,
                         BlimpConnectionStatistics* statistics);

  ~StreamSocketConnection() override;

 private:
  std::unique_ptr<net::StreamSocket> socket_;
};

}  // namespace blimp

#endif  // BLIMP_NET_STREAM_SOCKET_CONNECTION_H_
