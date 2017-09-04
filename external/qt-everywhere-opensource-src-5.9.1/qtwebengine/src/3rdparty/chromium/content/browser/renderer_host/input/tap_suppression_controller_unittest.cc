// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/tap_suppression_controller.h"

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace content {

class MockTapSuppressionController : public TapSuppressionController,
                                     public TapSuppressionControllerClient {
 public:
  using TapSuppressionController::DISABLED;
  using TapSuppressionController::NOTHING;
  using TapSuppressionController::GFC_IN_PROGRESS;
  using TapSuppressionController::TAP_DOWN_STASHED;
  using TapSuppressionController::LAST_CANCEL_STOPPED_FLING;

  enum Action {
    NONE                                 = 0,
    TAP_DOWN_DEFERRED                    = 1 << 0,
    TAP_DOWN_FORWARDED                   = 1 << 1,
    TAP_DOWN_DROPPED                     = 1 << 2,
    TAP_UP_SUPPRESSED                    = 1 << 3,
    TAP_UP_FORWARDED                     = 1 << 4,
    TAP_CANCEL_SUPPRESSED                = 1 << 5,
    TAP_CANCEL_FORWARDED                 = 1 << 6,
    STASHED_TAP_DOWN_FORWARDED           = 1 << 7,
  };

  MockTapSuppressionController(const TapSuppressionController::Config& config)
      : TapSuppressionController(this, config),
        last_actions_(NONE),
        time_(),
        timer_started_(false) {}

  ~MockTapSuppressionController() override {}

  void SendGestureFlingCancel() {
    last_actions_ = NONE;
    GestureFlingCancel();
  }

  void SendGestureFlingCancelAck(bool processed) {
    last_actions_ = NONE;
    GestureFlingCancelAck(processed);
  }

  void SendTapDown() {
    last_actions_ = NONE;
    if (ShouldDeferTapDown())
      last_actions_ |= TAP_DOWN_DEFERRED;
    else
      last_actions_ |= TAP_DOWN_FORWARDED;
  }

  void SendTapUp() {
    last_actions_ = NONE;
    if (ShouldSuppressTapEnd())
      last_actions_ |= TAP_UP_SUPPRESSED;
    else
      last_actions_ |= TAP_UP_FORWARDED;
  }

  void SendTapCancel() {
    last_actions_ = NONE;
    if (ShouldSuppressTapEnd())
      last_actions_ |= TAP_CANCEL_SUPPRESSED;
    else
      last_actions_ |= TAP_CANCEL_FORWARDED;
  }

  void AdvanceTime(const base::TimeDelta& delta) {
    last_actions_ = NONE;
    time_ += delta;
    if (timer_started_ && time_ >= timer_expiry_time_) {
      timer_started_ = false;
      TapDownTimerExpired();
    }
  }

  State state() { return state_; }

  int last_actions() { return last_actions_; }

 protected:
  base::TimeTicks Now() override { return time_; }

  void StartTapDownTimer(const base::TimeDelta& delay) override {
    timer_expiry_time_ = time_ + delay;
    timer_started_ = true;
  }

  void StopTapDownTimer() override { timer_started_ = false; }

 private:
  // TapSuppressionControllerClient implementation
  void DropStashedTapDown() override { last_actions_ |= TAP_DOWN_DROPPED; }

  void ForwardStashedTapDown() override {
    last_actions_ |= STASHED_TAP_DOWN_FORWARDED;
  }

  // Hiding some derived public methods
  using TapSuppressionController::GestureFlingCancel;
  using TapSuppressionController::GestureFlingCancelAck;
  using TapSuppressionController::ShouldDeferTapDown;
  using TapSuppressionController::ShouldSuppressTapEnd;

  int last_actions_;

  base::TimeTicks time_;
  bool timer_started_;
  base::TimeTicks timer_expiry_time_;

  DISALLOW_COPY_AND_ASSIGN(MockTapSuppressionController);
};

class TapSuppressionControllerTest : public testing::Test {
 public:
  TapSuppressionControllerTest() {
  }
  ~TapSuppressionControllerTest() override {}

 protected:
  // testing::Test
  void SetUp() override {
    tap_suppression_controller_.reset(
        new MockTapSuppressionController(GetConfig()));
  }

  void TearDown() override { tap_suppression_controller_.reset(); }

  static TapSuppressionController::Config GetConfig() {
    TapSuppressionController::Config config;
    config.enabled = true;
    config.max_cancel_to_down_time = base::TimeDelta::FromMilliseconds(10);
    config.max_tap_gap_time = base::TimeDelta::FromMilliseconds(10);
    return config;
  }

  std::unique_ptr<MockTapSuppressionController> tap_suppression_controller_;
};

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown and everything happens without any delays.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapFast) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a small delay between TapDown and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapInsufficientlyLateTapUp) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between TapDown and TapUp, so they are still
  // considered a tap.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a long delay between TapDown and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapSufficientlyLateTapUp) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send processed GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send MouseDown. This MouseDown should be suppressed, for now.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between TapDown and TapUp, so they are not
  // considered a tap. This should release the previously suppressed TapDown.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::STASHED_TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a small delay between the Ack and TapDown.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapInsufficientlyLateTapDown) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between GestureFlingCancel and TapDown, so the
  // TapDown is still considered associated with the GestureFlingCancel.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes before
// TapDown, but there is a long delay between the Ack and TapDown.
TEST_F(TapSuppressionControllerTest, GFCAckBeforeTapSufficientlyLateTapDown) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between GestureFlingCancel and TapDown, so the
  // TapDown is not considered associated with the GestureFlingCancel.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::LAST_CANCEL_STOPPED_FLING,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should not be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send MouseUp. This MouseUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when unprocessed GestureFlingCancel Ack
// comes after TapDown and everything happens without any delay.
TEST_F(TapSuppressionControllerTest, GFCAckUnprocessedAfterTapFast) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed, for now.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send unprocessed GestureFlingCancel Ack. This should release the
  // previously suppressed TapDown.
  tap_suppression_controller_->SendGestureFlingCancelAck(false);
  EXPECT_EQ(MockTapSuppressionController::STASHED_TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when processed GestureFlingCancel Ack comes
// after TapDown and everything happens without any delay.
TEST_F(TapSuppressionControllerTest, GFCAckProcessedAfterTapFast) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send processed GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes after
// TapDown and there is a small delay between the Ack and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckAfterTapInsufficientlyLateTapUp) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait less than allowed delay between TapDown and TapUp, so they are still
  // considered as a tap.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(7));
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_SUPPRESSED |
            MockTapSuppressionController::TAP_DOWN_DROPPED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test TapSuppressionController for when GestureFlingCancel Ack comes after
// TapDown and there is a long delay between the Ack and TapUp.
TEST_F(TapSuppressionControllerTest, GFCAckAfterTapSufficientlyLateTapUp) {
  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::GFC_IN_PROGRESS,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should be suppressed, for now.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_DEFERRED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_STASHED,
            tap_suppression_controller_->state());

  // Wait more than allowed delay between TapDown and TapUp, so they are not
  // considered as a tap. This should release the previously suppressed TapDown.
  tap_suppression_controller_->AdvanceTime(TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(MockTapSuppressionController::STASHED_TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::NOTHING,
            tap_suppression_controller_->state());
}

// Test that no suppression occurs if the TapSuppressionController is disabled.
TEST_F(TapSuppressionControllerTest, NoSuppressionIfDisabled) {
  TapSuppressionController::Config disabled_config;
  disabled_config.enabled = false;
  tap_suppression_controller_.reset(
      new MockTapSuppressionController(disabled_config));

  // Send GestureFlingCancel.
  tap_suppression_controller_->SendGestureFlingCancel();
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());

  // Send GestureFlingCancel Ack.
  tap_suppression_controller_->SendGestureFlingCancelAck(true);
  EXPECT_EQ(MockTapSuppressionController::NONE,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());

  // Send TapDown. This TapDown should not be suppressed.
  tap_suppression_controller_->SendTapDown();
  EXPECT_EQ(MockTapSuppressionController::TAP_DOWN_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());

  // Send TapUp. This TapUp should not be suppressed.
  tap_suppression_controller_->SendTapUp();
  EXPECT_EQ(MockTapSuppressionController::TAP_UP_FORWARDED,
            tap_suppression_controller_->last_actions());
  EXPECT_EQ(MockTapSuppressionController::DISABLED,
            tap_suppression_controller_->state());
}

}  // namespace content
