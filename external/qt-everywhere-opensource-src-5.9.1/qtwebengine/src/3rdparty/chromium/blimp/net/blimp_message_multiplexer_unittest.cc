// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/navigation.pb.h"
#include "blimp/net/blimp_message_multiplexer.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Ref;
using testing::SaveArg;

namespace blimp {
namespace {

class BlimpMessageMultiplexerTest : public testing::Test {
 public:
  BlimpMessageMultiplexerTest()
      : multiplexer_(&mock_output_processor_),
        input_message_(new BlimpMessage),
        navigation_message_(new BlimpMessage),
        input_processor_(multiplexer_.CreateSender(BlimpMessage::kInput)),
        navigation_processor_(
            multiplexer_.CreateSender(BlimpMessage::kNavigation)) {}

  void SetUp() override {
    EXPECT_CALL(mock_output_processor_, MockableProcessMessage(_, _))
        .WillRepeatedly(
            DoAll(SaveArg<0>(&captured_message_), SaveArg<1>(&captured_cb_)));

    input_message_->mutable_input()->set_type(
        InputMessage::Type_GestureScrollBegin);
    navigation_message_->mutable_navigation()->set_type(
        NavigationMessage::LOAD_URL);
  }

 protected:
  MockBlimpMessageProcessor mock_output_processor_;
  BlimpMessageMultiplexer multiplexer_;
  std::unique_ptr<BlimpMessage> input_message_;
  std::unique_ptr<BlimpMessage> navigation_message_;
  BlimpMessage captured_message_;
  net::CompletionCallback captured_cb_;
  std::unique_ptr<BlimpMessageProcessor> input_processor_;
  std::unique_ptr<BlimpMessageProcessor> navigation_processor_;
};

// Verify that each sender propagates its types and copies the message payload
// correctly.
TEST_F(BlimpMessageMultiplexerTest, TypeSetByMux) {
  net::TestCompletionCallback cb_1;
  input_processor_->ProcessMessage(std::move(input_message_), cb_1.callback());
  EXPECT_EQ(BlimpMessage::kInput, captured_message_.feature_case());
  EXPECT_EQ(InputMessage::Type_GestureScrollBegin,
            captured_message_.input().type());
  captured_cb_.Run(net::OK);
  EXPECT_EQ(net::OK, cb_1.WaitForResult());

  net::TestCompletionCallback cb_2;
  navigation_processor_->ProcessMessage(std::move(navigation_message_),
                                        cb_2.callback());
  EXPECT_EQ(BlimpMessage::kNavigation, captured_message_.feature_case());
  EXPECT_EQ(NavigationMessage::LOAD_URL, captured_message_.navigation().type());
  captured_cb_.Run(net::ERR_FAILED);
  EXPECT_EQ(net::ERR_FAILED, cb_2.WaitForResult());
}

// Verify that the multiplexer allows the caller to supply a message type.
TEST_F(BlimpMessageMultiplexerTest, TypeSetByCaller) {
  input_message_->mutable_input();

  net::TestCompletionCallback cb_1;
  input_processor_->ProcessMessage(std::move(input_message_), cb_1.callback());
  EXPECT_EQ(BlimpMessage::kInput, captured_message_.feature_case());
  EXPECT_EQ(InputMessage::Type_GestureScrollBegin,
            captured_message_.input().type());
  captured_cb_.Run(net::OK);
  EXPECT_EQ(net::OK, cb_1.WaitForResult());
}

// Verify that senders for a given type can be torn down and recreated.
TEST_F(BlimpMessageMultiplexerTest, SenderTransience) {
  net::TestCompletionCallback cb_3;
  input_processor_ = multiplexer_.CreateSender(BlimpMessage::kInput);
  input_processor_->ProcessMessage(std::move(input_message_), cb_3.callback());
  EXPECT_EQ(BlimpMessage::kInput, captured_message_.feature_case());
  EXPECT_EQ(InputMessage::Type_GestureScrollBegin,
            captured_message_.input().type());
  captured_cb_.Run(net::OK);
  EXPECT_EQ(net::OK, cb_3.WaitForResult());
}

// Verify that there is no limit on the number of senders for a given type.
TEST_F(BlimpMessageMultiplexerTest, SenderMultiplicity) {
  net::TestCompletionCallback cb_4;
  std::unique_ptr<BlimpMessageProcessor> input_processor_2 =
      multiplexer_.CreateSender(BlimpMessage::kInput);
  input_processor_2->ProcessMessage(std::move(input_message_), cb_4.callback());
  EXPECT_EQ(BlimpMessage::kInput, captured_message_.feature_case());
  EXPECT_EQ(InputMessage::Type_GestureScrollBegin,
            captured_message_.input().type());
  captured_cb_.Run(net::ERR_INVALID_ARGUMENT);
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, cb_4.WaitForResult());
}

}  // namespace
}  // namespace blimp
