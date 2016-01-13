// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_service.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/battery_status/battery_status_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"

namespace content {

namespace {

class FakeBatteryManager : public BatteryStatusManager {
 public:
  explicit FakeBatteryManager(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : start_invoked_count_(0),
        stop_invoked_count_(0) {
    callback_ = callback;
  }
  virtual ~FakeBatteryManager() { }

  // Methods from Battery Status Manager
  virtual bool StartListeningBatteryChange() OVERRIDE {
    start_invoked_count_++;
    return true;
  }

  virtual void StopListeningBatteryChange() OVERRIDE {
    stop_invoked_count_++;
  }

  void InvokeUpdateCallback(const blink::WebBatteryStatus& status) {
    callback_.Run(status);
  }

  int start_invoked_count() const { return start_invoked_count_; }
  int stop_invoked_count() const { return stop_invoked_count_; }

 private:
  int start_invoked_count_;
  int stop_invoked_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryManager);
};

class BatteryStatusServiceTest : public testing::Test {
 public:
    BatteryStatusServiceTest()
        : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
          battery_service_(0),
          battery_manager_(0),
          callback1_invoked_count_(0),
          callback2_invoked_count_(0) {
    }
    virtual ~BatteryStatusServiceTest() { }

 protected:
  typedef BatteryStatusService::BatteryUpdateSubscription BatterySubscription;

  virtual void SetUp() OVERRIDE {
    callback1_ = base::Bind(&BatteryStatusServiceTest::Callback1,
                            base::Unretained(this));
    callback2_ = base::Bind(&BatteryStatusServiceTest::Callback2,
                            base::Unretained(this));
    battery_service_ = BatteryStatusService::GetInstance();
    battery_manager_ = new FakeBatteryManager(
        battery_service_->GetUpdateCallbackForTesting());
    battery_service_->SetBatteryManagerForTesting(battery_manager_);
  }

  virtual void TearDown() OVERRIDE {
    base::RunLoop().RunUntilIdle();
    battery_service_->SetBatteryManagerForTesting(0);
  }

  FakeBatteryManager* battery_manager() {
    return battery_manager_;
  }

  scoped_ptr<BatterySubscription> AddCallback(
      const BatteryStatusService::BatteryUpdateCallback& callback) {
    return battery_service_->AddCallback(callback);
  }

  int callback1_invoked_count() const {
    return callback1_invoked_count_;
  }

  int callback2_invoked_count() const {
    return callback2_invoked_count_;
  }

  const blink::WebBatteryStatus& battery_status() const {
    return battery_status_;
  }

  const BatteryStatusService::BatteryUpdateCallback& callback1() const {
    return callback1_;
  }

  const BatteryStatusService::BatteryUpdateCallback& callback2() const {
    return callback2_;
  }

 private:
  void Callback1(const blink::WebBatteryStatus& status) {
    callback1_invoked_count_++;
    battery_status_ = status;
  }

  void Callback2(const blink::WebBatteryStatus& status) {
    callback2_invoked_count_++;
    battery_status_ = status;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  BatteryStatusService* battery_service_;
  FakeBatteryManager* battery_manager_;
  BatteryStatusService::BatteryUpdateCallback callback1_;
  BatteryStatusService::BatteryUpdateCallback callback2_;
  int callback1_invoked_count_;
  int callback2_invoked_count_;
  blink::WebBatteryStatus battery_status_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusServiceTest);
};

TEST_F(BatteryStatusServiceTest, AddFirstCallback) {
  scoped_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  EXPECT_EQ(1, battery_manager()->start_invoked_count());
  EXPECT_EQ(0, battery_manager()->stop_invoked_count());
  subscription1.reset();
  EXPECT_EQ(1, battery_manager()->start_invoked_count());
  EXPECT_EQ(1, battery_manager()->stop_invoked_count());
}

TEST_F(BatteryStatusServiceTest, AddCallbackAfterUpdate) {
  scoped_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  blink::WebBatteryStatus status;
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(0, callback2_invoked_count());

  scoped_ptr<BatterySubscription> subscription2 = AddCallback(callback2());
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(1, callback2_invoked_count());
}

TEST_F(BatteryStatusServiceTest, TwoCallbacksUpdate) {
  scoped_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  scoped_ptr<BatterySubscription> subscription2 = AddCallback(callback2());

  blink::WebBatteryStatus status;
  status.charging = true;
  status.chargingTime = 100;
  status.dischargingTime = 200;
  status.level = 0.5;
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(1, callback2_invoked_count());
  EXPECT_EQ(status.charging, battery_status().charging);
  EXPECT_EQ(status.chargingTime, battery_status().chargingTime);
  EXPECT_EQ(status.dischargingTime, battery_status().dischargingTime);
  EXPECT_EQ(status.level, battery_status().level);
}

TEST_F(BatteryStatusServiceTest, RemoveOneCallback) {
  scoped_ptr<BatterySubscription> subscription1 = AddCallback(callback1());
  scoped_ptr<BatterySubscription> subscription2 = AddCallback(callback2());

  blink::WebBatteryStatus status;
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(1, callback2_invoked_count());

  subscription1.reset();
  battery_manager()->InvokeUpdateCallback(status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback1_invoked_count());
  EXPECT_EQ(2, callback2_invoked_count());
}

}  // namespace

}  // namespace content
