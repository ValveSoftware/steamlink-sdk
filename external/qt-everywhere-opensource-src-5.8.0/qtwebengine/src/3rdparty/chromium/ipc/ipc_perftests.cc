// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_perftest_support.h"

namespace {

// This test times the roundtrip IPC message cycle.
//
// TODO(brettw): Make this test run by default.

class IPCChannelPerfTest : public IPC::test::IPCChannelPerfTestBase {
};

TEST_F(IPCChannelPerfTest, ChannelPingPong) {
  RunTestChannelPingPong(GetDefaultTestParams());
}

TEST_F(IPCChannelPerfTest, ChannelProxyPingPong) {
  RunTestChannelProxyPingPong(GetDefaultTestParams());
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(PerformanceClient) {
  IPC::test::PingPongTestClient client;
  return client.RunMain();
}

}  // namespace
