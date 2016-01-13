// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "battery_status_dispatcher.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/battery_status_messages.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

class MockBatteryStatusListener : public blink::WebBatteryStatusListener {
 public:
  MockBatteryStatusListener() : did_change_battery_status_(false) { }
  virtual ~MockBatteryStatusListener() { }

  // blink::WebBatteryStatusListener method.
  virtual void updateBatteryStatus(
      const blink::WebBatteryStatus& status) OVERRIDE {
    status_ = status;
    did_change_battery_status_ = true;
  }

  const blink::WebBatteryStatus& status() const { return status_; }
  bool did_change_battery_status() const { return did_change_battery_status_; }

 private:
  bool did_change_battery_status_;
  blink::WebBatteryStatus status_;

  DISALLOW_COPY_AND_ASSIGN(MockBatteryStatusListener);
};

class BatteryStatusDispatcherForTesting : public BatteryStatusDispatcher {
 public:
  BatteryStatusDispatcherForTesting()
      : BatteryStatusDispatcher(0),
        start_invoked_(false),
        stop_invoked_(false) { }

  virtual ~BatteryStatusDispatcherForTesting() { }

  bool start_invoked() const { return start_invoked_; }
  bool stop_invoked() const { return stop_invoked_; }

 protected:
  virtual bool Start() OVERRIDE {
    start_invoked_ = true;
    return true;
  }

  virtual bool Stop() OVERRIDE {
    stop_invoked_ = true;
    return true;
  }

 private:
  bool start_invoked_;
  bool stop_invoked_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusDispatcherForTesting);
};

TEST(BatteryStatusDispatcherTest, Start) {
  MockBatteryStatusListener listener;
  BatteryStatusDispatcherForTesting dispatcher;

  EXPECT_FALSE(dispatcher.start_invoked());
  EXPECT_FALSE(dispatcher.stop_invoked());

  dispatcher.SetListener(&listener);
  EXPECT_TRUE(dispatcher.start_invoked());

  dispatcher.SetListener(0);
  EXPECT_TRUE(dispatcher.stop_invoked());
}

TEST(BatteryStatusDispatcherTest, UpdateListener) {
  MockBatteryStatusListener listener;
  BatteryStatusDispatcherForTesting dispatcher;

  blink::WebBatteryStatus status;
  status.charging = true;
  status.chargingTime = 100;
  status.dischargingTime = 200;
  status.level = 0.5;

  dispatcher.SetListener(&listener);
  EXPECT_TRUE(dispatcher.start_invoked());

  BatteryStatusMsg_DidChange message(status);
  dispatcher.OnControlMessageReceived(message);

  const blink::WebBatteryStatus& received_status = listener.status();
  EXPECT_TRUE(listener.did_change_battery_status());
  EXPECT_EQ(status.charging, received_status.charging);
  EXPECT_EQ(status.chargingTime, received_status.chargingTime);
  EXPECT_EQ(status.dischargingTime, received_status.dischargingTime);
  EXPECT_EQ(status.level, received_status.level);

  dispatcher.SetListener(0);
  EXPECT_TRUE(dispatcher.stop_invoked());
}

TEST(BatteryStatusDispatcherTest, NoUpdateWhenNullListener) {
  MockBatteryStatusListener listener;
  BatteryStatusDispatcherForTesting dispatcher;

  dispatcher.SetListener(0);
  EXPECT_FALSE(dispatcher.start_invoked());
  EXPECT_TRUE(dispatcher.stop_invoked());

  blink::WebBatteryStatus status;
  BatteryStatusMsg_DidChange message(status);
  dispatcher.OnControlMessageReceived(message);
  EXPECT_FALSE(listener.did_change_battery_status());
}

}  // namespace content
