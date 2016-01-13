// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_client_peer.h"

#include "net/tools/quic/quic_client.h"

namespace net {
namespace tools {
namespace test {

// static
QuicCryptoClientConfig* QuicClientPeer::GetCryptoConfig(QuicClient* client) {
  return &client->crypto_config_;
}

// static
bool QuicClientPeer::CreateUDPSocket(QuicClient* client) {
  return client->CreateUDPSocket();
}

// static
void QuicClientPeer::SetClientPort(QuicClient* client, int port) {
  client->client_address_ = IPEndPoint(client->client_address_.address(), port);
}

}  // namespace test
}  // namespace tools
}  // namespace net
