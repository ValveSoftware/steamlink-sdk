// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/client_connection_manager.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/protocol_version.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock-more-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {
const char kDummyClientAuthToken[] = "dummy-client-token";
}  // namespace

class ClientConnectionManagerTest : public testing::Test {
 public:
  ClientConnectionManagerTest()
      : manager_(new ClientConnectionManager(&connection_handler_)),
        transport1_(new testing::StrictMock<MockTransport>),
        transport2_(new testing::StrictMock<MockTransport>),
        connection_(new testing::StrictMock<MockBlimpConnection>),
        start_connection_message_(
            CreateStartConnectionMessage(kDummyClientAuthToken,
                                         kProtocolVersion)),
        message_capture_(new testing::StrictMock<MockBlimpMessageProcessor>) {
    manager_->set_client_auth_token(kDummyClientAuthToken);
  }

  ~ClientConnectionManagerTest() override {}

 protected:
  base::MessageLoop message_loop_;
  testing::StrictMock<MockConnectionHandler> connection_handler_;
  std::unique_ptr<ClientConnectionManager> manager_;
  std::unique_ptr<testing::StrictMock<MockTransport>> transport1_;
  std::unique_ptr<testing::StrictMock<MockTransport>> transport2_;
  std::unique_ptr<testing::StrictMock<MockBlimpConnection>> connection_;
  std::unique_ptr<BlimpMessage> start_connection_message_;
  std::unique_ptr<testing::StrictMock<MockBlimpMessageProcessor>>
      message_capture_;
};

// Tests that the transport connection works.
TEST_F(ClientConnectionManagerTest, FirstTransportConnects) {
  net::CompletionCallback write_cb;
  net::CompletionCallback connect_cb_1;
  EXPECT_CALL(*transport1_, Connect(_)).WillOnce(SaveArg<0>(&connect_cb_1));
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(_));
  EXPECT_CALL(
      *message_capture_,
      MockableProcessMessage(EqualsProto(*start_connection_message_), _))
      .WillOnce(SaveArg<1>(&write_cb));
  EXPECT_CALL(*connection_, AddConnectionErrorObserver(_));
  EXPECT_CALL(*connection_, GetOutgoingMessageProcessor())
      .WillOnce(Return(message_capture_.get()));

  transport1_->SetMockConnection(std::move(connection_));
  EXPECT_TRUE(connect_cb_1.is_null());
  manager_->AddTransport(std::move(transport1_));
  manager_->AddTransport(std::move(transport2_));
  manager_->Connect();
  EXPECT_FALSE(connect_cb_1.is_null());
  base::ResetAndReturn(&connect_cb_1).Run(net::OK);
  base::ResetAndReturn(&write_cb).Run(net::OK);
}

// The 1st transport fails to connect, and the 2nd transport connects.
TEST_F(ClientConnectionManagerTest, SecondTransportConnects) {
  net::CompletionCallback write_cb;
  net::CompletionCallback connect_cb_1;
  EXPECT_CALL(*transport1_, Connect(_)).WillOnce(SaveArg<0>(&connect_cb_1));
  net::CompletionCallback connect_cb_2;
  EXPECT_CALL(*transport2_, Connect(_)).WillOnce(SaveArg<0>(&connect_cb_2));
  EXPECT_CALL(
      *message_capture_,
      MockableProcessMessage(EqualsProto(*start_connection_message_), _))
      .WillOnce(SaveArg<1>(&write_cb));
  EXPECT_CALL(*connection_, AddConnectionErrorObserver(_));
  EXPECT_CALL(*connection_, GetOutgoingMessageProcessor())
      .WillOnce(Return(message_capture_.get()));

  BlimpConnection* actual_connection = nullptr;
  BlimpConnection* expected_connection = connection_.get();
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(_))
      .WillOnce(SaveArg<0>(&actual_connection));

  transport2_->SetMockConnection(std::move(connection_));

  EXPECT_TRUE(connect_cb_1.is_null());
  EXPECT_TRUE(connect_cb_2.is_null());
  manager_->AddTransport(std::move(transport1_));
  manager_->AddTransport(std::move(transport2_));
  manager_->Connect();
  EXPECT_FALSE(connect_cb_1.is_null());
  base::ResetAndReturn(&connect_cb_1).Run(net::ERR_FAILED);
  EXPECT_FALSE(connect_cb_2.is_null());
  base::ResetAndReturn(&connect_cb_2).Run(net::OK);
  base::ResetAndReturn(&write_cb).Run(net::OK);
  EXPECT_EQ(expected_connection, actual_connection);
}

// Both transports fail to connect.
TEST_F(ClientConnectionManagerTest, BothTransportsFailToConnect) {
  net::CompletionCallback connect_cb_1;
  EXPECT_CALL(*transport1_, Connect(_)).WillOnce(SaveArg<0>(&connect_cb_1));
  net::CompletionCallback connect_cb_2;
  EXPECT_CALL(*transport2_, Connect(_)).WillOnce(SaveArg<0>(&connect_cb_2));

  EXPECT_TRUE(connect_cb_1.is_null());
  EXPECT_TRUE(connect_cb_2.is_null());
  manager_->AddTransport(std::move(transport1_));
  manager_->AddTransport(std::move(transport2_));
  manager_->Connect();
  EXPECT_FALSE(connect_cb_1.is_null());
  EXPECT_TRUE(connect_cb_2.is_null());
  base::ResetAndReturn(&connect_cb_1).Run(net::ERR_FAILED);
  EXPECT_FALSE(connect_cb_2.is_null());
  base::ResetAndReturn(&connect_cb_2).Run(net::ERR_FAILED);
}

}  // namespace blimp
