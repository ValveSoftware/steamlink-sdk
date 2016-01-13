// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/power_profiler/power_profiler_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kNumEvents = 3;
const int kDefaultSamplePeriodMs = 50;

// Provide a set number of power events.
class TestPowerDataProvider : public PowerDataProvider {
 public:
  TestPowerDataProvider(int count) : num_events_to_send_(count) {}
  virtual ~TestPowerDataProvider() {}

  virtual PowerEventVector GetData() OVERRIDE {
    PowerEventVector events;
    if (num_events_to_send_ == 0)
      return events;

    PowerEvent event;
    event.type = PowerEvent::SOC_PACKAGE;
    event.time = base::TimeTicks::Now();
    event.value = 1.0;
    events.push_back(event);

    num_events_to_send_--;
    return events;
  }

  virtual base::TimeDelta GetSamplingRate() OVERRIDE {
    return base::TimeDelta::FromMilliseconds(kDefaultSamplePeriodMs);
  }

 private:
  int num_events_to_send_;
  DISALLOW_COPY_AND_ASSIGN(TestPowerDataProvider);
};

class TestPowerProfilerObserver : public PowerProfilerObserver {
 public:
  TestPowerProfilerObserver()
      : valid_event_count_(0),
        total_num_events_received_(0) {}
  virtual ~TestPowerProfilerObserver() {}

  virtual void OnPowerEvent(const PowerEventVector& events) OVERRIDE {
    if (IsValidEvent(events[0]))
      ++valid_event_count_;

    total_num_events_received_++;
    if (total_num_events_received_ >= kNumEvents) {
      // All expected events received, exiting.
      quit_closure_.Run();
    }
  }

  int valid_event_count() const { return valid_event_count_; }
  void set_quit_closure(base::Closure closure) { quit_closure_ = closure; }

 private:
  bool IsValidEvent(const PowerEvent& event) {
    return event.type == PowerEvent::SOC_PACKAGE &&
           !event.time.is_null() &&
           event.value > 0;
  }

  int valid_event_count_;
  int total_num_events_received_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestPowerProfilerObserver);
};

} // namespace

class PowerProfilerServiceTest : public testing::Test {
 public:
  void ServiceStartTest() {
    service_.reset(new PowerProfilerService(
        make_scoped_ptr<PowerDataProvider>(
            new TestPowerDataProvider(kNumEvents)),
        message_loop_.message_loop_proxy(),
        base::TimeDelta::FromMilliseconds(1)));
    EXPECT_TRUE(service_->IsAvailable());
  }

  void AddObserverTest() {
    service_->AddObserver(&observer_);

    // No PowerEvents received.
    EXPECT_EQ(observer_.valid_event_count(), 0);
  }

  void RemoveObserverTest() {
     service_->RemoveObserver(&observer_);

    // Received |kNumEvents| events.
    EXPECT_EQ(observer_.valid_event_count(), kNumEvents);
  }

 protected:
  PowerProfilerServiceTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~PowerProfilerServiceTest() {}

  void RegisterQuitClosure(base::Closure closure) {
    observer_.set_quit_closure(closure);
  }

 private:
  scoped_ptr<PowerProfilerService> service_;
  TestPowerProfilerObserver observer_;

  // UI thread.
  base::MessageLoopForUI message_loop_;
  BrowserThreadImpl ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(PowerProfilerServiceTest);
};

// Test whether PowerProfilerService dispatches power events to observer
// properly.
TEST_F(PowerProfilerServiceTest, AvailableService) {
  base::RunLoop run_loop;
  RegisterQuitClosure(run_loop.QuitClosure());

  ServiceStartTest();
  AddObserverTest();

  run_loop.Run();

  RemoveObserverTest();
}

}  // namespace content
