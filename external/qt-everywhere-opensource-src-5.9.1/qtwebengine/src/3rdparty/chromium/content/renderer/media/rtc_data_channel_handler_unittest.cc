// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_data_channel_handler.h"

#include <stddef.h>

#include <memory>

#include "base/test/test_simple_task_runner.h"
#include "content/renderer/media/mock_data_channel_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"

namespace content {

class MockDataChannelHandlerClient :
    public blink::WebRTCDataChannelHandlerClient {
 public:
  MockDataChannelHandlerClient() : state_(ReadyStateConnecting) {}

  void didChangeReadyState(ReadyState state) override { state_ = state; }
  void didDecreaseBufferedAmount(unsigned previous_amount) override {}
  void didReceiveStringData(const blink::WebString& s) override {}
  void didReceiveRawData(const char* data, size_t size) override {}
  void didDetectError() override {}

  ReadyState ready_state() const { return state_; }

 private:
  ReadyState state_;
};

class RtcDataChannelHandlerTest : public ::testing::Test {
 public:
  RtcDataChannelHandlerTest() {
    signaling_thread_ = new base::TestSimpleTaskRunner();
  }

  void SetUp() override {
    channel_ = new rtc::RefCountedObject<MockDataChannel>("test", &config);
  }

  void TearDown() override {
    handler_.reset();
    channel_ = nullptr;
    signaling_thread_->ClearPendingTasks();
  }

  webrtc::DataChannelInit config;
  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread_;
  scoped_refptr<MockDataChannel> channel_;
  std::unique_ptr<RtcDataChannelHandler> handler_;
};

// Add a client, change to the open state, and verify that the client has
// reached the open state.
TEST_F(RtcDataChannelHandlerTest, SetClient) {
  handler_.reset(new RtcDataChannelHandler(signaling_thread_, channel_.get()));
  MockDataChannelHandlerClient blink_channel;
  handler_->setClient(&blink_channel);
  channel_->changeState(webrtc::DataChannelInterface::kOpen);
  signaling_thread_->RunPendingTasks();
  EXPECT_EQ(MockDataChannelHandlerClient::ReadyStateOpen,
      blink_channel.ready_state());
}

// Check that state() returns the expected default initial value.
TEST_F(RtcDataChannelHandlerTest, InitialState) {
  handler_.reset(new RtcDataChannelHandler(signaling_thread_, channel_.get()));
  EXPECT_EQ(MockDataChannelHandlerClient::ReadyStateConnecting,
      handler_->state());
}

// Check that state() returns the expected value when the channel opens early.
TEST_F(RtcDataChannelHandlerTest, StateEarlyOpen) {
  channel_->changeState(webrtc::DataChannelInterface::kOpen);
  signaling_thread_->RunPendingTasks();
  handler_.reset(new RtcDataChannelHandler(signaling_thread_, channel_.get()));
  EXPECT_EQ(MockDataChannelHandlerClient::ReadyStateOpen,
      handler_->state());
}

}  // namespace content
