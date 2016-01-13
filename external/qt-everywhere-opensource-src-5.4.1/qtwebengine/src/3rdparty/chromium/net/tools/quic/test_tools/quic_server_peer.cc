// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_server_peer.h"

#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_server.h"

namespace net {
namespace tools {
namespace test {

// static
bool QuicServerPeer::SetSmallSocket(QuicServer* server) {
  int size = 1024 * 10;
  return setsockopt(
      server->fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) != -1;
}

// static
void QuicServerPeer::DisableRecvmmsg(QuicServer* server) {
  server->use_recvmmsg_ = false;
}

// static
QuicDispatcher* QuicServerPeer::GetDispatcher(QuicServer* server) {
  return server->dispatcher_.get();
}

}  // namespace test
}  // namespace tools
}  // namespace net
