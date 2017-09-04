// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class WebFrameSchedulerImplTest : public testing::Test {
 public:
  WebFrameSchedulerImplTest() {}
  ~WebFrameSchedulerImplTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::WrapUnique(new TestTimeSource(clock_.get())));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    web_view_scheduler_.reset(
        new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));
    web_frame_scheduler_ =
        web_view_scheduler_->createWebFrameSchedulerImpl(nullptr);
  }

  void TearDown() override {
    web_frame_scheduler_.reset();
    web_view_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler_;
};

namespace {
class RepeatingTask : public blink::WebTaskRunner::Task {
 public:
  RepeatingTask(blink::WebTaskRunner* web_task_runner, int* run_count)
      : web_task_runner_(web_task_runner), run_count_(run_count) {}

  ~RepeatingTask() override {}

  void run() override {
    (*run_count_)++;
    web_task_runner_->postDelayedTask(
        BLINK_FROM_HERE, new RepeatingTask(web_task_runner_, run_count_), 1.0);
  }

 private:
  blink::WebTaskRunner* web_task_runner_;  // NOT OWNED
  int* run_count_;                         // NOT OWNED
};
}  // namespace

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_PageInForeground) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_PageInBackground) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(true);
  web_view_scheduler_->setPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_SameOrigin) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(true);
  web_frame_scheduler_->setFrameVisible(false);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameVisible_CrossOrigin) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(true);
  web_frame_scheduler_->setFrameVisible(true);
  web_frame_scheduler_->setCrossOrigin(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_CrossOrigin) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(true);
  web_frame_scheduler_->setFrameVisible(false);
  web_frame_scheduler_->setCrossOrigin(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, PageInBackground_ThrottlingDisabled) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(false);
  web_view_scheduler_->setPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_CrossOrigin_ThrottlingDisabled) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(false);
  web_frame_scheduler_->setFrameVisible(false);
  web_frame_scheduler_->setCrossOrigin(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

}  // namespace scheduler
}  // namespace blink
