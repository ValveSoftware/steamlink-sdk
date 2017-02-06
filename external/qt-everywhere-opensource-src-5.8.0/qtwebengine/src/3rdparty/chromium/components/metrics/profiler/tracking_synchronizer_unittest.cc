// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/tracking_synchronizer.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/tracked_objects.h"
#include "components/metrics/profiler/tracking_synchronizer_delegate.h"
#include "components/metrics/profiler/tracking_synchronizer_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using tracked_objects::ProcessDataPhaseSnapshot;
using tracked_objects::TaskSnapshot;

namespace metrics {

namespace {

class TestDelegate : public TrackingSynchronizerDelegate {
 public:
  ~TestDelegate() override {}

  static std::unique_ptr<TrackingSynchronizerDelegate> Create(
      TrackingSynchronizer* synchronizer) {
    return base::WrapUnique(new TestDelegate());
  }

 private:
  TestDelegate() {}

  // TrackingSynchronizerDelegate:
  void GetProfilerDataForChildProcesses(int sequence_number,
                                        int current_profiling_phase) override {}
  void OnProfilingPhaseCompleted(int profiling_phase) override {}
};

class TestObserver : public TrackingSynchronizerObserver {
 public:
  TestObserver() {}

  ~TestObserver() override {
    EXPECT_TRUE(got_phase_0_);
    EXPECT_TRUE(got_phase_1_);
  }

  void ReceivedProfilerData(
      const ProfilerDataAttributes& attributes,
      const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
      const ProfilerEvents& past_events) override {
    EXPECT_EQ(static_cast<base::ProcessId>(239), attributes.process_id);
    EXPECT_EQ(ProfilerEventProto::TrackedObject::PPAPI_PLUGIN,
              attributes.process_type);
    ASSERT_EQ(1u, process_data_phase.tasks.size());

    switch (attributes.profiling_phase) {
      case 0:
        EXPECT_FALSE(got_phase_0_);
        got_phase_0_ = true;

        EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromMilliseconds(111),
                  attributes.phase_start);
        EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromMilliseconds(333),
                  attributes.phase_finish);

        EXPECT_EQ("death_thread0",
                  process_data_phase.tasks[0].death_thread_name);
        EXPECT_EQ(0u, past_events.size());
        break;

      case 1:
        EXPECT_FALSE(got_phase_1_);
        got_phase_1_ = true;

        EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromMilliseconds(333),
                  attributes.phase_start);
        EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromMilliseconds(777),
                  attributes.phase_finish);

        EXPECT_EQ("death_thread1",
                  process_data_phase.tasks[0].death_thread_name);
        ASSERT_EQ(1u, past_events.size());
        EXPECT_EQ(ProfilerEventProto::EVENT_FIRST_NONEMPTY_PAINT,
                  past_events[0]);
        break;

      default:
        bool profiling_phase_is_neither_0_nor_1 = true;
        EXPECT_FALSE(profiling_phase_is_neither_0_nor_1);
    }
  }

 private:
  bool got_phase_0_ = false;
  bool got_phase_1_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestTrackingSynchronizer : public TrackingSynchronizer {
 public:
  explicit TestTrackingSynchronizer(std::unique_ptr<base::TickClock> clock)
      : TrackingSynchronizer(std::move(clock),
                             base::Bind(&TestDelegate::Create)) {}

  using TrackingSynchronizer::RegisterPhaseCompletion;
  using TrackingSynchronizer::SendData;

 private:
  ~TestTrackingSynchronizer() override {}
};

}  // namespace

TEST(TrackingSynchronizerTest, ProfilerData) {
  // Testing how TrackingSynchronizer reports 2 phases of profiling.
  auto clock = new base::SimpleTestTickClock();  // Will be owned by
                                                 // |tracking_synchronizer|.
  clock->Advance(base::TimeDelta::FromMilliseconds(111));

  scoped_refptr<TestTrackingSynchronizer> tracking_synchronizer =
      new TestTrackingSynchronizer(base::WrapUnique(clock));

  clock->Advance(base::TimeDelta::FromMilliseconds(222));

  tracking_synchronizer->RegisterPhaseCompletion(
      ProfilerEventProto::EVENT_FIRST_NONEMPTY_PAINT);

  tracked_objects::ProcessDataSnapshot profiler_data;
  ProcessDataPhaseSnapshot snapshot0;
  tracked_objects::TaskSnapshot task_snapshot0;
  task_snapshot0.death_thread_name = "death_thread0";
  snapshot0.tasks.push_back(task_snapshot0);
  ProcessDataPhaseSnapshot snapshot1;
  profiler_data.phased_snapshots[0] = snapshot0;
  tracked_objects::TaskSnapshot task_snapshot1;
  task_snapshot1.death_thread_name = "death_thread1";
  snapshot1.tasks.push_back(task_snapshot1);
  profiler_data.phased_snapshots[1] = snapshot1;
  profiler_data.process_id = 239;

  clock->Advance(base::TimeDelta::FromMilliseconds(444));
  TestObserver test_observer;
  tracking_synchronizer->SendData(
      profiler_data, ProfilerEventProto::TrackedObject::PPAPI_PLUGIN,
      &test_observer);
}

}  // namespace metrics
