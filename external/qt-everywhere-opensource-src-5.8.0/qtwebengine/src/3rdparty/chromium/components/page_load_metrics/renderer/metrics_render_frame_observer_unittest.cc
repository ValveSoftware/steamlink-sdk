// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/fake_page_timing_metrics_ipc_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

// Implementation of the MetricsRenderFrameObserver class we're testing,
// with the GetTiming() and ShouldSendMetrics() methods stubbed out to make
// the rest of the class more testable.
class TestMetricsRenderFrameObserver : public MetricsRenderFrameObserver {
 public:
  TestMetricsRenderFrameObserver() : MetricsRenderFrameObserver(nullptr) {}

  std::unique_ptr<base::Timer> CreateTimer() const override {
    if (!mock_timer_)
      ADD_FAILURE() << "CreateTimer() called, but no MockTimer available.";
    return std::move(mock_timer_);
  }

  // We intercept sent messages and dispatch them to our
  // FakePageTimingMetricsIPCSender, which we use to verify that the expected
  // IPC messages get sent.
  bool Send(IPC::Message* message) override {
    return fake_timing_ipc_sender_.Send(message);
  }

  void set_mock_timer(std::unique_ptr<base::Timer> timer) {
    ASSERT_EQ(nullptr, mock_timer_);
    mock_timer_ = std::move(timer);
  }

  void ExpectPageLoadTiming(const PageLoadTiming& timing) {
    fake_timing_ipc_sender_.ExpectPageLoadTiming(timing);
  }

  PageLoadTiming GetTiming() const override {
    return fake_timing_ipc_sender_.expected_timings().empty()
               ? PageLoadTiming()
               : fake_timing_ipc_sender_.expected_timings().back();
  }

  void VerifyExpectedTimings() const {
    fake_timing_ipc_sender_.VerifyExpectedTimings();
  }

  bool ShouldSendMetrics() const override { return true; }
  bool HasNoRenderFrame() const override { return false; }

 private:
  FakePageTimingMetricsIPCSender fake_timing_ipc_sender_;
  mutable std::unique_ptr<base::Timer> mock_timer_;
};

typedef testing::Test MetricsRenderFrameObserverTest;

TEST_F(MetricsRenderFrameObserverTest, NoMetrics) {
  TestMetricsRenderFrameObserver observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  observer.DidChangePerformanceTiming();
  ASSERT_FALSE(mock_timer->IsRunning());
}

TEST_F(MetricsRenderFrameObserverTest, SingleMetric) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(10);

  TestMetricsRenderFrameObserver observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.DidCommitProvisionalLoad(true, false);
  mock_timer->Fire();

  timing.first_layout = first_layout;
  observer.ExpectPageLoadTiming(timing);

  observer.DidChangePerformanceTiming();
  mock_timer->Fire();
}

TEST_F(MetricsRenderFrameObserverTest, MultipleMetrics) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta dom_event = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(2);

  TestMetricsRenderFrameObserver observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.DidCommitProvisionalLoad(true, false);
  mock_timer->Fire();

  timing.first_layout = first_layout;
  timing.dom_content_loaded_event_start = dom_event;
  observer.ExpectPageLoadTiming(timing);

  observer.DidChangePerformanceTiming();
  mock_timer->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  observer.VerifyExpectedTimings();

  timing.load_event_start = load_event;
  observer.ExpectPageLoadTiming(timing);

  observer.DidChangePerformanceTiming();
  mock_timer->Fire();

  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  observer.VerifyExpectedTimings();

  // The PageLoadTiming above includes timing information for the first layout,
  // dom content, and load metrics. However, since we've already generated
  // timing information for all of these metrics previously, we do not expect
  // this invocation to generate any additional metrics.
  observer.DidChangePerformanceTiming();
  ASSERT_FALSE(mock_timer->IsRunning());
}

TEST_F(MetricsRenderFrameObserverTest, MultipleNavigations) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta dom_event = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(2);

  TestMetricsRenderFrameObserver observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.DidCommitProvisionalLoad(true, false);
  mock_timer->Fire();

  timing.first_layout = first_layout;
  timing.dom_content_loaded_event_start = dom_event;
  timing.load_event_start = load_event;
  observer.ExpectPageLoadTiming(timing);
  observer.DidChangePerformanceTiming();
  mock_timer->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  observer.VerifyExpectedTimings();

  base::Time nav_start_2 = base::Time::FromDoubleT(100);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMillisecondsD(20);
  base::TimeDelta dom_event_2 = base::TimeDelta::FromMillisecondsD(20);
  base::TimeDelta load_event_2 = base::TimeDelta::FromMillisecondsD(20);
  PageLoadTiming timing_2;
  timing_2.navigation_start = nav_start_2;

  base::MockTimer* mock_timer2 = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer2));

  observer.ExpectPageLoadTiming(timing_2);
  observer.DidCommitProvisionalLoad(true, false);
  mock_timer2->Fire();

  timing_2.first_layout = first_layout_2;
  timing_2.dom_content_loaded_event_start = dom_event_2;
  timing_2.load_event_start = load_event_2;
  observer.ExpectPageLoadTiming(timing_2);

  observer.DidChangePerformanceTiming();
  mock_timer2->Fire();
}

}  // namespace page_load_metrics
