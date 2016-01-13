// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_

#include "net/tools/quic/quic_dispatcher.h"

#include "net/base/ip_endpoint.h"

namespace net {
namespace tools {

class QuicPacketWriterWrapper;

namespace test {

class QuicDispatcherPeer {
 public:
  static void SetTimeWaitListManager(
      QuicDispatcher* dispatcher,
      QuicTimeWaitListManager* time_wait_list_manager);

  // Injects |writer| into |dispatcher| as the top level writer.
  static void UseWriter(QuicDispatcher* dispatcher,
                        QuicPacketWriterWrapper* writer);

  static QuicPacketWriter* GetWriter(QuicDispatcher* dispatcher);

  static QuicEpollConnectionHelper* GetHelper(QuicDispatcher* dispatcher);

  static QuicConnection* CreateQuicConnection(
      QuicDispatcher* dispatcher,
      QuicConnectionId connection_id,
      const IPEndPoint& server,
      const IPEndPoint& client);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicDispatcherPeer);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_
