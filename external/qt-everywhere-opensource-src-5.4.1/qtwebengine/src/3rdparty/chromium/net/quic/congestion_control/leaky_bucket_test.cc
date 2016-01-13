// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class LeakyBucketTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    leaky_bucket_.reset(new LeakyBucket(QuicBandwidth::Zero()));
  }
  MockClock clock_;
  scoped_ptr<LeakyBucket> leaky_bucket_;
};

TEST_F(LeakyBucketTest, Basic) {
  QuicBandwidth draining_rate = QuicBandwidth::FromBytesPerSecond(200000);
  leaky_bucket_->SetDrainingRate(clock_.Now(), draining_rate);
  leaky_bucket_->Add(clock_.Now(), 2000);
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining(clock_.Now()));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(5),
            leaky_bucket_->TimeRemaining(clock_.Now()));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_TRUE(leaky_bucket_->TimeRemaining(clock_.Now()).IsZero());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_TRUE(leaky_bucket_->TimeRemaining(clock_.Now()).IsZero());
  leaky_bucket_->Add(clock_.Now(), 2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(11));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_TRUE(leaky_bucket_->TimeRemaining(clock_.Now()).IsZero());
  leaky_bucket_->Add(clock_.Now(), 2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  leaky_bucket_->Add(clock_.Now(), 2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining(clock_.Now()));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_TRUE(leaky_bucket_->TimeRemaining(clock_.Now()).IsZero());
}

TEST_F(LeakyBucketTest, ChangeDrainRate) {
  QuicBandwidth draining_rate = QuicBandwidth::FromBytesPerSecond(200000);
  leaky_bucket_->SetDrainingRate(clock_.Now(), draining_rate);
  leaky_bucket_->Add(clock_.Now(), 2000);
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining(clock_.Now()));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(5),
            leaky_bucket_->TimeRemaining(clock_.Now()));
  draining_rate = draining_rate.Scale(0.5f);  // Cut drain rate in half.
  leaky_bucket_->SetDrainingRate(clock_.Now(), draining_rate);
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending(clock_.Now()));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining(clock_.Now()));
}

}  // namespace test
}  // namespace net
