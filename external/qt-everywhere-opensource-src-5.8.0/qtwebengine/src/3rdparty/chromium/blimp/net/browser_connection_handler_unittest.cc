// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/browser_connection_handler.h"

#include <stddef.h>

#include <string>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

// Compares two blimp messages and ignores message_id field.
MATCHER_P(EqualsMessageIgnoringId, message, "") {
  BlimpMessage expected_message = message;
  expected_message.clear_message_id();
  BlimpMessage actual_message = arg;
  actual_message.clear_message_id();

  std::string expected_serialized;
  std::string actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  actual_message.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class FakeFeature {
 public:
  FakeFeature(BlimpMessage::FeatureCase feature_case,
              BrowserConnectionHandler* connection_handler) {
    outgoing_message_processor_ = connection_handler->RegisterFeature(
        feature_case, &incoming_message_processor_);
  }

  ~FakeFeature() {}

  BlimpMessageProcessor* outgoing_message_processor() {
    return outgoing_message_processor_.get();
  }

  MockBlimpMessageProcessor* incoming_message_processor() {
    return &incoming_message_processor_;
  }

 private:
  testing::StrictMock<MockBlimpMessageProcessor> incoming_message_processor_;
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;
};

class FakeBlimpConnection : public BlimpConnection,
                            public BlimpMessageProcessor {
 public:
  FakeBlimpConnection() {}
  ~FakeBlimpConnection() override {}

  void set_other_end(FakeBlimpConnection* other_end) { other_end_ = other_end; }

  ConnectionErrorObserver* error_observer() { return error_observer_; }

  // BlimpConnection implementation.
  void AddConnectionErrorObserver(ConnectionErrorObserver* observer) override {
    error_observer_ = observer;
  }

  void SetIncomingMessageProcessor(BlimpMessageProcessor* processor) override {
    incoming_message_processor_ = processor;
  }

  BlimpMessageProcessor* GetOutgoingMessageProcessor() override { return this; }

 private:
  void ForwardMessage(std::unique_ptr<BlimpMessage> message) {
    other_end_->incoming_message_processor_->ProcessMessage(
        std::move(message), net::CompletionCallback());
  }

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&FakeBlimpConnection::ForwardMessage,
                              base::Unretained(this), base::Passed(&message)));

    callback.Run(net::OK);
  }

  FakeBlimpConnection* other_end_ = nullptr;
  ConnectionErrorObserver* error_observer_ = nullptr;
  BlimpMessageProcessor* incoming_message_processor_ = nullptr;
};

std::unique_ptr<BlimpMessage> CreateInputMessage(int tab_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->mutable_input();
  output->set_target_tab_id(tab_id);
  return output;
}

std::unique_ptr<BlimpMessage> CreateControlMessage(int tab_id) {
  std::unique_ptr<BlimpMessage> output(new BlimpMessage);
  output->mutable_tab_control();
  output->set_target_tab_id(tab_id);
  return output;
}

class BrowserConnectionHandlerTest : public testing::Test {
 public:
  BrowserConnectionHandlerTest()
      : client_connection_handler_(new BrowserConnectionHandler),
        engine_connection_handler_(new BrowserConnectionHandler) {
    SetupConnections();

    client_input_feature_.reset(new FakeFeature(
        BlimpMessage::kInput, client_connection_handler_.get()));
    engine_input_feature_.reset(new FakeFeature(
        BlimpMessage::kInput, engine_connection_handler_.get()));
    client_control_feature_.reset(new FakeFeature(
        BlimpMessage::kTabControl, client_connection_handler_.get()));
    engine_control_feature_.reset(new FakeFeature(
        BlimpMessage::kTabControl, engine_connection_handler_.get()));
  }

  ~BrowserConnectionHandlerTest() override {}
  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  void SetupConnections() {
    client_connection_ = new FakeBlimpConnection();
    engine_connection_ = new FakeBlimpConnection();
    client_connection_->set_other_end(engine_connection_);
    engine_connection_->set_other_end(client_connection_);
    client_connection_handler_->HandleConnection(
        base::WrapUnique(client_connection_));
    engine_connection_handler_->HandleConnection(
        base::WrapUnique(engine_connection_));
  }

  base::MessageLoop message_loop_;

  FakeBlimpConnection* client_connection_;
  FakeBlimpConnection* engine_connection_;

  std::unique_ptr<BrowserConnectionHandler> client_connection_handler_;
  std::unique_ptr<BrowserConnectionHandler> engine_connection_handler_;

  std::unique_ptr<FakeFeature> client_input_feature_;
  std::unique_ptr<FakeFeature> engine_input_feature_;
  std::unique_ptr<FakeFeature> client_control_feature_;
  std::unique_ptr<FakeFeature> engine_control_feature_;
};

TEST_F(BrowserConnectionHandlerTest, ExchangeMessages) {
  std::unique_ptr<BlimpMessage> client_input_message = CreateInputMessage(1);
  std::unique_ptr<BlimpMessage> client_control_message =
      CreateControlMessage(1);
  std::unique_ptr<BlimpMessage> engine_control_message =
      CreateControlMessage(2);

  EXPECT_CALL(
      *(engine_input_feature_->incoming_message_processor()),
      MockableProcessMessage(EqualsMessageIgnoringId(*client_input_message), _))
      .RetiresOnSaturation();
  EXPECT_CALL(*(engine_control_feature_->incoming_message_processor()),
              MockableProcessMessage(
                  EqualsMessageIgnoringId(*client_control_message), _))
      .RetiresOnSaturation();
  EXPECT_CALL(*(client_control_feature_->incoming_message_processor()),
              MockableProcessMessage(
                  EqualsMessageIgnoringId(*engine_control_message), _))
      .RetiresOnSaturation();

  client_input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_input_message), net::CompletionCallback());
  client_control_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_control_message), net::CompletionCallback());
  engine_control_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(engine_control_message), net::CompletionCallback());
}

TEST_F(BrowserConnectionHandlerTest, ConnectionError) {
  // Engine will not get message after connection error.
  client_connection_->error_observer()->OnConnectionError(net::ERR_FAILED);
  std::unique_ptr<BlimpMessage> client_input_message = CreateInputMessage(1);
  client_input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_input_message), net::CompletionCallback());
}

TEST_F(BrowserConnectionHandlerTest, ReconnectionAfterError) {
  std::unique_ptr<BlimpMessage> client_input_message = CreateInputMessage(1);
  EXPECT_CALL(
      *(engine_input_feature_->incoming_message_processor()),
      MockableProcessMessage(EqualsMessageIgnoringId(*client_input_message), _))
      .RetiresOnSaturation();

  // Simulate a connection failure.
  client_connection_->error_observer()->OnConnectionError(net::ERR_FAILED);
  engine_connection_->error_observer()->OnConnectionError(net::ERR_FAILED);

  // Message will be queued to be transmitted when the connection is
  // re-established.
  client_input_feature_->outgoing_message_processor()->ProcessMessage(
      std::move(client_input_message), net::CompletionCallback());

  // Simulates reconnection.
  SetupConnections();
}

}  // namespace
}  // namespace blimp
