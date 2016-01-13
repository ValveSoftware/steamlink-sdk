// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_DISPATCHER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_DISPATCHER_H_

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace tools {
namespace test {

class MockQuicDispatcher : public QuicDispatcher {
 public:
  MockQuicDispatcher(const QuicConfig& config,
                     const QuicCryptoServerConfig& crypto_config,
                     EpollServer* eps);

  virtual ~MockQuicDispatcher();

  MOCK_METHOD3(ProcessPacket, void(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   const QuicEncryptedPacket& packet));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockQuicDispatcher);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_MOCK_QUIC_DISPATCHER_H_
