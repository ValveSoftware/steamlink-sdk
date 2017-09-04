// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/engine_connection_manager.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/tcp_client_transport.h"
#include "blimp/net/tcp_connection.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Return;
using testing::SaveArg;

namespace blimp {

class EngineConnectionManagerTest : public testing::Test {
 public:
  EngineConnectionManagerTest()
      : manager_(new EngineConnectionManager(&connection_handler_, nullptr)) {}

  ~EngineConnectionManagerTest() override {}

 protected:
  testing::StrictMock<MockConnectionHandler> connection_handler_;
  std::unique_ptr<EngineConnectionManager> manager_;
  base::MessageLoopForIO message_loop_;
};

TEST_F(EngineConnectionManagerTest, ConnectionSucceeds) {
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(_)).Times(1);

  net::IPAddress ip_addr(net::IPAddress::IPv4Localhost());
  net::IPEndPoint engine_endpoint(ip_addr, 0);

  manager_->ConnectTransport(&engine_endpoint,
                             EngineConnectionManager::EngineTransportType::TCP);

  net::TestCompletionCallback connect_callback;
  TCPClientTransport client(engine_endpoint, nullptr);
  client.Connect(connect_callback.callback());
  EXPECT_EQ(net::OK, connect_callback.WaitForResult());
}

TEST_F(EngineConnectionManagerTest, TwoConnectionsSucceed) {
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(_)).Times(2);

  net::IPAddress ip_addr(net::IPAddress::IPv4Localhost());
  net::IPEndPoint engine_endpoint(ip_addr, 0);

  manager_->ConnectTransport(&engine_endpoint,
                             EngineConnectionManager::EngineTransportType::TCP);

  net::TestCompletionCallback connect_callback;
  TCPClientTransport client(engine_endpoint, nullptr);

  client.Connect(connect_callback.callback());
  EXPECT_EQ(net::OK, connect_callback.WaitForResult());
  std::unique_ptr<BlimpConnection> connection = client.MakeConnection();

  client.Connect(connect_callback.callback());
  EXPECT_EQ(net::OK, connect_callback.WaitForResult());
}

}  // namespace blimp
