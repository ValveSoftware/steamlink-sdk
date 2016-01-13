// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_alarm.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::Invoke;

namespace net {
namespace test {
namespace {

class MockDelegate : public QuicAlarm::Delegate {
 public:
  MOCK_METHOD0(OnAlarm, QuicTime());
};

class TestAlarm : public QuicAlarm {
 public:
  explicit TestAlarm(QuicAlarm::Delegate* delegate) : QuicAlarm(delegate) {}

  bool scheduled() const { return scheduled_; }

  void FireAlarm() {
    scheduled_ = false;
    Fire();
  }

 protected:
  virtual void SetImpl() OVERRIDE {
    DCHECK(deadline().IsInitialized());
    scheduled_ = true;
  }

  virtual void CancelImpl() OVERRIDE {
    DCHECK(!deadline().IsInitialized());
    scheduled_ = false;
  }

 private:
  bool scheduled_;
};

class QuicAlarmTest : public ::testing::Test {
 public:
  QuicAlarmTest()
      : delegate_(new MockDelegate()),
        alarm_(delegate_),
        deadline_(QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(7))),
        deadline2_(QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(14))),
        new_deadline_(QuicTime::Zero()) {
  }

  void ResetAlarm() {
    alarm_.Set(new_deadline_);
  }

  MockDelegate* delegate_;  // not owned
  TestAlarm alarm_;
  QuicTime deadline_;
  QuicTime deadline2_;
  QuicTime new_deadline_;
};

TEST_F(QuicAlarmTest, IsSet) {
  EXPECT_FALSE(alarm_.IsSet());
}

TEST_F(QuicAlarmTest, Set) {
  QuicTime deadline = QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(7));
  alarm_.Set(deadline);
  EXPECT_TRUE(alarm_.IsSet());
  EXPECT_TRUE(alarm_.scheduled());
  EXPECT_EQ(deadline, alarm_.deadline());
}

TEST_F(QuicAlarmTest, Cancel) {
  QuicTime deadline = QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(7));
  alarm_.Set(deadline);
  alarm_.Cancel();
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());
}

TEST_F(QuicAlarmTest, Fire) {
  QuicTime deadline = QuicTime::Zero().Add(QuicTime::Delta::FromSeconds(7));
  alarm_.Set(deadline);
  EXPECT_CALL(*delegate_, OnAlarm()).WillOnce(Return(QuicTime::Zero()));
  alarm_.FireAlarm();
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());
}

TEST_F(QuicAlarmTest, FireAndResetViaReturn) {
  alarm_.Set(deadline_);
  EXPECT_CALL(*delegate_, OnAlarm()).WillOnce(Return(deadline2_));
  alarm_.FireAlarm();
  EXPECT_TRUE(alarm_.IsSet());
  EXPECT_TRUE(alarm_.scheduled());
  EXPECT_EQ(deadline2_, alarm_.deadline());
}

TEST_F(QuicAlarmTest, FireAndResetViaSet) {
  alarm_.Set(deadline_);
  new_deadline_ = deadline2_;
  EXPECT_CALL(*delegate_, OnAlarm()).WillOnce(DoAll(
      Invoke(this, &QuicAlarmTest::ResetAlarm),
      Return(QuicTime::Zero())));
  alarm_.FireAlarm();
  EXPECT_TRUE(alarm_.IsSet());
  EXPECT_TRUE(alarm_.scheduled());
  EXPECT_EQ(deadline2_, alarm_.deadline());
}

}  // namespace
}  // namespace test
}  // namespace net
