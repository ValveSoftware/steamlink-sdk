// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flow_controller.h"

#include "base/strings/stringprintf.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_flow_controller_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPrintf;

namespace net {
namespace test {

using ::testing::_;

class QuicFlowControllerTest : public ::testing::Test {
 public:
  QuicFlowControllerTest()
      : stream_id_(1234),
        send_window_(kInitialSessionFlowControlWindowForTest),
        receive_window_(kInitialSessionFlowControlWindowForTest),
        max_receive_window_(kInitialSessionFlowControlWindowForTest),
        connection_(false),
        old_flag_(&FLAGS_enable_quic_stream_flow_control_2, true) {
  }

  void Initialize() {
    flow_controller_.reset(new QuicFlowController(
        &connection_, stream_id_, false, send_window_,
        receive_window_, max_receive_window_));
  }

 protected:
  QuicStreamId stream_id_;
  uint64 send_window_;
  uint64 receive_window_;
  uint64 max_receive_window_;
  scoped_ptr<QuicFlowController> flow_controller_;
  MockConnection connection_;
  ValueRestore<bool> old_flag_;
};

TEST_F(QuicFlowControllerTest, SendingBytes) {
  Initialize();

  EXPECT_TRUE(flow_controller_->IsEnabled());
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_FALSE(flow_controller_->FlowControlViolation());
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());

  // Send some bytes, but not enough to block.
  flow_controller_->AddBytesSent(send_window_ / 2);
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_EQ(send_window_ / 2, flow_controller_->SendWindowSize());

  // Send enough bytes to block.
  flow_controller_->AddBytesSent(send_window_ / 2);
  EXPECT_TRUE(flow_controller_->IsBlocked());
  EXPECT_EQ(0u, flow_controller_->SendWindowSize());

  // BLOCKED frame should get sent.
  EXPECT_CALL(connection_, SendBlocked(stream_id_)).Times(1);
  flow_controller_->MaybeSendBlocked();

  // Update the send window, and verify this has unblocked.
  EXPECT_TRUE(flow_controller_->UpdateSendWindowOffset(2 * send_window_));
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());

  // Updating with a smaller offset doesn't change anything.
  EXPECT_FALSE(flow_controller_->UpdateSendWindowOffset(send_window_ / 10));
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());

  // Try to send more bytes, violating flow control.
  EXPECT_CALL(connection_,
              SendConnectionClose(QUIC_FLOW_CONTROL_SENT_TOO_MUCH_DATA));
  EXPECT_DFATAL(
      flow_controller_->AddBytesSent(send_window_ * 10),
      StringPrintf("Trying to send an extra %d bytes",
                   static_cast<int>(send_window_ * 10)));
  EXPECT_TRUE(flow_controller_->IsBlocked());
  EXPECT_EQ(0u, flow_controller_->SendWindowSize());
}

TEST_F(QuicFlowControllerTest, ReceivingBytes) {
  Initialize();

  EXPECT_TRUE(flow_controller_->IsEnabled());
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_FALSE(flow_controller_->FlowControlViolation());
  EXPECT_EQ(kInitialSessionFlowControlWindowForTest,
            QuicFlowControllerPeer::ReceiveWindowSize(flow_controller_.get()));

  // Receive some bytes, updating highest received offset, but not enough to
  // fill flow control receive window.
  EXPECT_TRUE(
      flow_controller_->UpdateHighestReceivedOffset(1 + receive_window_ / 2));
  EXPECT_FALSE(flow_controller_->FlowControlViolation());
  EXPECT_EQ((receive_window_ / 2) - 1,
            QuicFlowControllerPeer::ReceiveWindowSize(flow_controller_.get()));

  // Consume enough bytes to send a WINDOW_UPDATE frame.
  EXPECT_CALL(connection_, SendWindowUpdate(stream_id_, _)).Times(1);

  flow_controller_->AddBytesConsumed(1 + receive_window_ / 2);

  // Result is that once again we have a fully open receive window.
  EXPECT_FALSE(flow_controller_->FlowControlViolation());
  EXPECT_EQ(kInitialSessionFlowControlWindowForTest,
            QuicFlowControllerPeer::ReceiveWindowSize(flow_controller_.get()));
}

TEST_F(QuicFlowControllerTest,
       DisabledWhenQuicVersionDoesNotSupportFlowControl) {
  // Only support version 16: no flow control.
  QuicConnectionPeer::SetSupportedVersions(&connection_,
                                           SupportedVersions(QUIC_VERSION_16));

  Initialize();

  MockConnection connection(false);

  // Should not be enabled, and should not report as blocked.
  EXPECT_FALSE(flow_controller_->IsEnabled());
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_FALSE(flow_controller_->FlowControlViolation());

  // Any attempts to add/remove bytes should have no effect.
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());
  EXPECT_EQ(send_window_,
            QuicFlowControllerPeer::SendWindowOffset(flow_controller_.get()));
  EXPECT_EQ(receive_window_, QuicFlowControllerPeer::ReceiveWindowOffset(
                                 flow_controller_.get()));
  flow_controller_->AddBytesSent(123);
  flow_controller_->AddBytesConsumed(456);
  flow_controller_->UpdateHighestReceivedOffset(789);
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());
  EXPECT_EQ(send_window_,
            QuicFlowControllerPeer::SendWindowOffset(flow_controller_.get()));
  EXPECT_EQ(receive_window_, QuicFlowControllerPeer::ReceiveWindowOffset(
                                 flow_controller_.get()));

  // Any attempt to change offset should have no effect.
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());
  EXPECT_EQ(send_window_,
            QuicFlowControllerPeer::SendWindowOffset(flow_controller_.get()));
  flow_controller_->UpdateSendWindowOffset(send_window_ + 12345);
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());
  EXPECT_EQ(send_window_,
            QuicFlowControllerPeer::SendWindowOffset(flow_controller_.get()));

  // The connection should never send WINDOW_UPDATE or BLOCKED frames, even if
  // the internal state implies that it should.

  // If the flow controller was enabled, then a send window size of 0 would
  // trigger a BLOCKED frame to be sent.
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());
  EXPECT_CALL(connection_, SendBlocked(_)).Times(0);
  flow_controller_->MaybeSendBlocked();

  // If the flow controller was enabled, then a WINDOW_UPDATE would be sent if
  // (receive window) < (max receive window / 2)
  QuicFlowControllerPeer::SetReceiveWindowOffset(flow_controller_.get(),
                                                 max_receive_window_ / 10);
  EXPECT_TRUE(QuicFlowControllerPeer::ReceiveWindowSize(
                  flow_controller_.get()) < (max_receive_window_ / 2));
  EXPECT_CALL(connection_, SendWindowUpdate(_, _)).Times(0);
  flow_controller_->AddBytesConsumed(0);

  // Should not be enabled, and should not report as blocked.
  EXPECT_FALSE(flow_controller_->IsEnabled());
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_FALSE(flow_controller_->FlowControlViolation());
}

TEST_F(QuicFlowControllerTest, OnlySendBlockedFrameOncePerOffset) {
  Initialize();

  // Test that we don't send duplicate BLOCKED frames. We should only send one
  // BLOCKED frame at a given send window offset.
  EXPECT_TRUE(flow_controller_->IsEnabled());
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_FALSE(flow_controller_->FlowControlViolation());
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());

  // Send enough bytes to block.
  flow_controller_->AddBytesSent(send_window_);
  EXPECT_TRUE(flow_controller_->IsBlocked());
  EXPECT_EQ(0u, flow_controller_->SendWindowSize());

  // Expect that 2 BLOCKED frames should get sent in total.
  EXPECT_CALL(connection_, SendBlocked(stream_id_)).Times(2);

  // BLOCKED frame should get sent.
  flow_controller_->MaybeSendBlocked();

  // BLOCKED frame should not get sent again until our send offset changes.
  flow_controller_->MaybeSendBlocked();
  flow_controller_->MaybeSendBlocked();
  flow_controller_->MaybeSendBlocked();
  flow_controller_->MaybeSendBlocked();
  flow_controller_->MaybeSendBlocked();

  // Update the send window, then send enough bytes to block again.
  EXPECT_TRUE(flow_controller_->UpdateSendWindowOffset(2 * send_window_));
  EXPECT_FALSE(flow_controller_->IsBlocked());
  EXPECT_EQ(send_window_, flow_controller_->SendWindowSize());
  flow_controller_->AddBytesSent(send_window_);
  EXPECT_TRUE(flow_controller_->IsBlocked());
  EXPECT_EQ(0u, flow_controller_->SendWindowSize());

  // BLOCKED frame should get sent as send offset has changed.
  flow_controller_->MaybeSendBlocked();
}

}  // namespace test
}  // namespace net
