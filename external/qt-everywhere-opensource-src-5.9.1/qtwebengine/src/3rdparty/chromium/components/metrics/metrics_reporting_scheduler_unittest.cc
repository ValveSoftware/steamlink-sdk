// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_scheduler.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class MetricsReportingSchedulerTest : public testing::Test {
 public:
  MetricsReportingSchedulerTest() : callback_call_count_(0) {}
  ~MetricsReportingSchedulerTest() override {}

  base::Closure GetCallback() {
    return base::Bind(&MetricsReportingSchedulerTest::SchedulerCallback,
                      base::Unretained(this));
  }

  base::Callback<base::TimeDelta(void)> GetConnectionCallback() {
    return base::Bind(&MetricsReportingSchedulerTest::GetStandardUploadInterval,
                      base::Unretained(this));
  }

  int callback_call_count() const { return callback_call_count_; }

 private:
  void SchedulerCallback() {
    ++callback_call_count_;
  }

  base::TimeDelta GetStandardUploadInterval() {
    return base::TimeDelta::FromMinutes(5);
  }

  int callback_call_count_;

  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MetricsReportingSchedulerTest);
};


TEST_F(MetricsReportingSchedulerTest, InitTaskCompleteBeforeTimer) {
  MetricsReportingScheduler scheduler(GetCallback(), GetConnectionCallback());
  scheduler.SetUploadIntervalForTesting(base::TimeDelta());
  scheduler.InitTaskComplete();
  scheduler.Start();
  EXPECT_EQ(0, callback_call_count());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_call_count());
}

TEST_F(MetricsReportingSchedulerTest, InitTaskCompleteAfterTimer) {
  MetricsReportingScheduler scheduler(GetCallback(), GetConnectionCallback());
  scheduler.SetUploadIntervalForTesting(base::TimeDelta());
  scheduler.Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_call_count());

  scheduler.InitTaskComplete();
  EXPECT_EQ(1, callback_call_count());
}

}  // namespace metrics
