// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/daily_event.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const char kTestPrefName[] = "TestPref";
const char kTestMetricName[] = "TestMetric";

class TestDailyObserver : public DailyEvent::Observer {
 public:
  TestDailyObserver() : fired_(false) {}

  bool fired() const { return fired_; }

  void OnDailyEvent() override { fired_ = true; }

  void Reset() {
    fired_ = false;
  }

 private:
  // True if this event has been observed.
  bool fired_;

  DISALLOW_COPY_AND_ASSIGN(TestDailyObserver);
};

class DailyEventTest : public testing::Test {
 public:
  DailyEventTest() : event_(&prefs_, kTestPrefName, kTestMetricName) {
    DailyEvent::RegisterPref(prefs_.registry(), kTestPrefName);
    observer_ = new TestDailyObserver();
    event_.AddObserver(base::WrapUnique(observer_));
  }

 protected:
  TestingPrefServiceSimple prefs_;
  TestDailyObserver* observer_;
  DailyEvent event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DailyEventTest);
};

}  // namespace

// The event should fire if the preference is not available.
TEST_F(DailyEventTest, TestNewFires) {
  event_.CheckInterval();
  EXPECT_TRUE(observer_->fired());
}

// The event should fire if the preference is more than a day old.
TEST_F(DailyEventTest, TestOldFires) {
  base::Time last_time = base::Time::Now() - base::TimeDelta::FromHours(25);
  prefs_.SetInt64(kTestPrefName, last_time.ToInternalValue());
  event_.CheckInterval();
  EXPECT_TRUE(observer_->fired());
}

// The event should fire if the preference is more than a day in the future.
TEST_F(DailyEventTest, TestFutureFires) {
  base::Time last_time = base::Time::Now() + base::TimeDelta::FromHours(25);
  prefs_.SetInt64(kTestPrefName, last_time.ToInternalValue());
  event_.CheckInterval();
  EXPECT_TRUE(observer_->fired());
}

// The event should not fire if the preference is more recent than a day.
TEST_F(DailyEventTest, TestRecentNotFired) {
  base::Time last_time = base::Time::Now() - base::TimeDelta::FromMinutes(2);
  prefs_.SetInt64(kTestPrefName, last_time.ToInternalValue());
  event_.CheckInterval();
  EXPECT_FALSE(observer_->fired());
}

// The event should not fire if the preference is less than a day in the future.
TEST_F(DailyEventTest, TestSoonNotFired) {
  base::Time last_time = base::Time::Now() + base::TimeDelta::FromMinutes(2);
  prefs_.SetInt64(kTestPrefName, last_time.ToInternalValue());
  event_.CheckInterval();
  EXPECT_FALSE(observer_->fired());
}

}  // namespace metrics
