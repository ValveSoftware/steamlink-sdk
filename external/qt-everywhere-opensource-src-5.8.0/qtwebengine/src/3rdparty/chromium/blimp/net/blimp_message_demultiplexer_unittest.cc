// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_demultiplexer.h"

#include "base/logging.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeArgument;
using testing::Ref;
using testing::Return;
using testing::SaveArg;

namespace blimp {

class BlimpMessageDemultiplexerTest : public testing::Test {
 public:
  BlimpMessageDemultiplexerTest()
      : input_msg_(new BlimpMessage), compositor_msg_(new BlimpMessage) {}

  void SetUp() override {
    demux_.AddProcessor(BlimpMessage::kInput, &receiver1_);
    demux_.AddProcessor(BlimpMessage::kCompositor, &receiver2_);
    InputMessage* input = nullptr;
    input_msg_ = CreateBlimpMessage(&input);
    CompositorMessage* compositor = nullptr;
    compositor_msg_ = CreateBlimpMessage(&compositor, 1);
  }

 protected:
  std::unique_ptr<BlimpMessage> input_msg_;
  std::unique_ptr<BlimpMessage> compositor_msg_;
  MockBlimpMessageProcessor receiver1_;
  MockBlimpMessageProcessor receiver2_;
  net::CompletionCallback captured_cb_;
  BlimpMessageDemultiplexer demux_;
};

TEST_F(BlimpMessageDemultiplexerTest, ProcessMessageOK) {
  EXPECT_CALL(receiver1_, MockableProcessMessage(Ref(*input_msg_), _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  net::TestCompletionCallback cb;
  demux_.ProcessMessage(std::move(input_msg_), cb.callback());
  captured_cb_.Run(net::OK);
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

TEST_F(BlimpMessageDemultiplexerTest, ProcessMessageFailed) {
  EXPECT_CALL(receiver2_, MockableProcessMessage(Ref(*compositor_msg_), _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  net::TestCompletionCallback cb2;
  demux_.ProcessMessage(std::move(compositor_msg_), cb2.callback());
  captured_cb_.Run(net::ERR_FAILED);
  EXPECT_EQ(net::ERR_FAILED, cb2.WaitForResult());
}

TEST_F(BlimpMessageDemultiplexerTest, ProcessMessageNoRegisteredHandler) {
  net::TestCompletionCallback cb;
  std::unique_ptr<BlimpMessage> unknown_message(new BlimpMessage);
  demux_.ProcessMessage(std::move(unknown_message), cb.callback());
  EXPECT_EQ(net::ERR_NOT_IMPLEMENTED, cb.WaitForResult());
}

}  // namespace blimp
