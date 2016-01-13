// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/heartbeat_manager.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

mcs_proto::HeartbeatConfig BuildHeartbeatConfig(int interval_ms) {
  mcs_proto::HeartbeatConfig config;
  config.set_interval_ms(interval_ms);
  return config;
}

class TestHeartbeatManager : public HeartbeatManager {
 public:
  TestHeartbeatManager() {}
  virtual ~TestHeartbeatManager() {}

  // Bypass the heartbeat timer, and send the heartbeat now.
  void TriggerHearbeat();
};

void TestHeartbeatManager::TriggerHearbeat() {
  OnHeartbeatTriggered();
}

class HeartbeatManagerTest : public testing::Test {
 public:
  HeartbeatManagerTest();
  virtual ~HeartbeatManagerTest() {}

  TestHeartbeatManager* manager() const { return manager_.get(); }
  int heartbeats_sent() const { return heartbeats_sent_; }
  int reconnects_triggered() const { return reconnects_triggered_; }

  // Starts the heartbeat manager.
  void StartManager();

 private:
  // Helper functions for verifying heartbeat manager effects.
  void SendHeartbeatClosure();
  void TriggerReconnectClosure();

  scoped_ptr<TestHeartbeatManager> manager_;

  int heartbeats_sent_;
  int reconnects_triggered_;

  base::MessageLoop message_loop_;
};

HeartbeatManagerTest::HeartbeatManagerTest()
    : manager_(new TestHeartbeatManager()),
      heartbeats_sent_(0),
      reconnects_triggered_(0) {
}

void HeartbeatManagerTest::StartManager() {
  manager_->Start(base::Bind(&HeartbeatManagerTest::SendHeartbeatClosure,
                             base::Unretained(this)),
                  base::Bind(&HeartbeatManagerTest::TriggerReconnectClosure,
                             base::Unretained(this)));
}

void HeartbeatManagerTest::SendHeartbeatClosure() {
  heartbeats_sent_++;
}

void HeartbeatManagerTest::TriggerReconnectClosure() {
  reconnects_triggered_++;
}

// Basic initialization. No heartbeat should be pending.
TEST_F(HeartbeatManagerTest, Init) {
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
}

// Acknowledging a heartbeat before starting the manager should have no effect.
TEST_F(HeartbeatManagerTest, AckBeforeStart) {
  manager()->OnHeartbeatAcked();
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
}

// Starting the manager should start the heartbeat timer.
TEST_F(HeartbeatManagerTest, Start) {
  StartManager();
  EXPECT_GT(manager()->GetNextHeartbeatTime(), base::TimeTicks::Now());
  EXPECT_EQ(0, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());
}

// Acking the heartbeat should trigger a new heartbeat timer.
TEST_F(HeartbeatManagerTest, AckedHeartbeat) {
  StartManager();
  manager()->TriggerHearbeat();
  base::TimeTicks heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_GT(heartbeat, base::TimeTicks::Now());
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());

  manager()->OnHeartbeatAcked();
  EXPECT_LT(heartbeat, manager()->GetNextHeartbeatTime());
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());

  manager()->TriggerHearbeat();
  EXPECT_EQ(2, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());
}

// Trigger a heartbeat when one was outstanding should reset the connection.
TEST_F(HeartbeatManagerTest, UnackedHeartbeat) {
  StartManager();
  manager()->TriggerHearbeat();
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());

  manager()->TriggerHearbeat();
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(1, reconnects_triggered());
}

// Updating the heartbeat interval before starting should result in the new
// interval being used at Start time.
TEST_F(HeartbeatManagerTest, UpdateIntervalThenStart) {
  const int kIntervalMs = 60 * 1000;  // 60 seconds.
  manager()->UpdateHeartbeatConfig(BuildHeartbeatConfig(kIntervalMs));
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
  StartManager();
  EXPECT_LE(manager()->GetNextHeartbeatTime() - base::TimeTicks::Now(),
            base::TimeDelta::FromMilliseconds(kIntervalMs));
}

// Updating the heartbeat interval after starting should only use the new
// interval on the next heartbeat.
TEST_F(HeartbeatManagerTest, StartThenUpdateInterval) {
  const int kIntervalMs = 60 * 1000;  // 60 seconds.
  StartManager();
  base::TimeTicks heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_GT(heartbeat - base::TimeTicks::Now(),
            base::TimeDelta::FromMilliseconds(kIntervalMs));

  // Updating the interval should not affect an outstanding heartbeat.
  manager()->UpdateHeartbeatConfig(BuildHeartbeatConfig(kIntervalMs));
  EXPECT_EQ(heartbeat, manager()->GetNextHeartbeatTime());

  // Triggering and acking the heartbeat should result in a heartbeat being
  // posted with the new interval.
  manager()->TriggerHearbeat();
  manager()->OnHeartbeatAcked();

  EXPECT_LE(manager()->GetNextHeartbeatTime() - base::TimeTicks::Now(),
            base::TimeDelta::FromMilliseconds(kIntervalMs));
  EXPECT_NE(heartbeat, manager()->GetNextHeartbeatTime());
}

// Stopping the manager should reset the heartbeat timer.
TEST_F(HeartbeatManagerTest, Stop) {
  StartManager();
  EXPECT_GT(manager()->GetNextHeartbeatTime(), base::TimeTicks::Now());

  manager()->Stop();
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
}

}  // namespace

}  // namespace gcm
