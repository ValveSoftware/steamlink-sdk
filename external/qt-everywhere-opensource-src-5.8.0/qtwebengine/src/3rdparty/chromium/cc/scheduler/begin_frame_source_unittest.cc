// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/begin_frame_source.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/begin_frame_source_test.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace cc {
namespace {

class BackToBackBeginFrameSourceTest : public ::testing::Test {
 protected:
  static const int64_t kDeadline;
  static const int64_t kInterval;

  void SetUp() override {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    task_runner_ =
        make_scoped_refptr(new OrderedSimpleTaskRunner(now_src_.get(), false));
    std::unique_ptr<TestDelayBasedTimeSource> time_source(
        new TestDelayBasedTimeSource(now_src_.get(), task_runner_.get()));
    source_.reset(new BackToBackBeginFrameSource(std::move(time_source)));
    obs_ = base::WrapUnique(new ::testing::StrictMock<MockBeginFrameObserver>);
  }

  void TearDown() override { obs_.reset(); }

  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;
  std::unique_ptr<BackToBackBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
};

const int64_t BackToBackBeginFrameSourceTest::kDeadline =
    BeginFrameArgs::DefaultInterval().ToInternalValue();

const int64_t BackToBackBeginFrameSourceTest::kInterval =
    BeginFrameArgs::DefaultInterval().ToInternalValue();

TEST_F(BackToBackBeginFrameSourceTest, AddObserverSendsBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenRemoveObserverProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get(), 0);

  // Verify no BeginFrame is sent to |obs_|. There is a pending task in the
  // task_runner_ as a BeginFrame was posted, but it gets aborted since |obs_|
  // is removed.
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTasks());
}

TEST_F(BackToBackBeginFrameSourceTest,
       RemoveObserverThenDidFinishFrameProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  source_->RemoveObserver(obs_.get());

  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest,
       TogglingObserverThenDidFinishFrameProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->RemoveObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  source_->DidFinishFrame(obs_.get(), 0);

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  // The begin frame is posted at the time when the observer was added,
  // so it ignores changes to "now" afterward.
  EXPECT_BEGIN_FRAME_USED(*obs_, 1110, 1110 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenTogglingObserverProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  source_->RemoveObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  // Ticks at the time at which the observer was added, ignoring the
  // last change to "now".
  EXPECT_BEGIN_FRAME_USED(*obs_, 1120, 1120 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameNoObserver) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_FALSE(task_runner_->RunPendingTasks());
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameRemainingFrames) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  // Runs the pending begin frame.
  task_runner_->RunPendingTasks();
  // While running the begin frame, the next frame was cancelled, this
  // runs the next frame, sees it was cancelled, and goes to sleep.
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTasks());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));

  source_->DidFinishFrame(obs_.get(), 3);
  EXPECT_FALSE(task_runner_->HasPendingTasks());
  source_->DidFinishFrame(obs_.get(), 2);
  EXPECT_FALSE(task_runner_->HasPendingTasks());
  source_->DidFinishFrame(obs_.get(), 1);
  EXPECT_FALSE(task_runner_->HasPendingTasks());

  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_EQ(base::TimeDelta(), task_runner_->DelayToNextTaskTime());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameMultipleCallsIdempotent) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_BEGIN_FRAME_USED(*obs_, 1200, 1200 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, DelayInPostedTaskProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(50));
  // Ticks at the time the last frame finished, so ignores the last change to
  // "now".
  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);

  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversSynchronized) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs1, 1000, 1000 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);
  source_->DidFinishFrame(&obs2, 0);
  EXPECT_BEGIN_FRAME_USED(obs1, 1100, 1100 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);
  source_->DidFinishFrame(&obs2, 0);
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  source_->RemoveObserver(&obs1);
  source_->RemoveObserver(&obs2);
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversInterleaved) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_USED(obs1, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs2, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);
  EXPECT_BEGIN_FRAME_USED(obs1, 1200, 1200 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->DidFinishFrame(&obs1, 0);
  source_->RemoveObserver(&obs1);
  // Finishing the frame for |obs1| posts a begin frame task, which will be
  // aborted since |obs1| is removed. Clear that from the task runner.
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs2, 0);
  EXPECT_BEGIN_FRAME_USED(obs2, 1300, 1300 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->DidFinishFrame(&obs2, 0);
  source_->RemoveObserver(&obs2);
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversAtOnce) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs1);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs1, 1000, 1000 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  // |obs1| finishes first.
  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);

  // |obs2| finishes also, before getting to the newly posted begin frame.
  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs2, 0);

  // Because the begin frame source already ticked when |obs1| finished,
  // we see it as the frame time for both observers.
  EXPECT_BEGIN_FRAME_USED(obs1, 1100, 1100 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->DidFinishFrame(&obs1, 0);
  source_->RemoveObserver(&obs1);
  source_->DidFinishFrame(&obs2, 0);
  source_->RemoveObserver(&obs2);
}

// DelayBasedBeginFrameSource testing ------------------------------------------
class DelayBasedBeginFrameSourceTest : public ::testing::Test {
 public:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;
  std::unique_ptr<DelayBasedBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;

  void SetUp() override {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    task_runner_ =
        make_scoped_refptr(new OrderedSimpleTaskRunner(now_src_.get(), false));
    std::unique_ptr<DelayBasedTimeSource> time_source(
        new TestDelayBasedTimeSource(now_src_.get(), task_runner_.get()));
    time_source->SetTimebaseAndInterval(
        base::TimeTicks(), base::TimeDelta::FromMicroseconds(10000));
    source_.reset(new DelayBasedBeginFrameSource(std::move(time_source)));
    obs_.reset(new MockBeginFrameObserver);
  }

  void TearDown() override { obs_.reset(); }
};

TEST_F(DelayBasedBeginFrameSourceTest,
       AddObserverCallsOnBeginFrameWithMissedTick) {
  now_src_->Advance(base::TimeDelta::FromMicroseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 10000, 20000, 10000);
  source_->AddObserver(obs_.get());  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.
}

TEST_F(DelayBasedBeginFrameSourceTest, AddObserverCallsCausesOnBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 10000, 10000);
  source_->AddObserver(obs_.get());
  EXPECT_EQ(10000, task_runner_->NextTaskTime().ToInternalValue());

  EXPECT_BEGIN_FRAME_USED(*obs_, 10000, 20000, 10000);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(9010));
  task_runner_->RunPendingTasks();
}

TEST_F(DelayBasedBeginFrameSourceTest, BasicOperation) {
  task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 10000, 10000);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 30000, 40000, 10000);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(30001));

  source_->RemoveObserver(obs_.get());
  // No new frames....
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(60000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChanges) {
  task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 10000, 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 30000, 40000, 10000);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(30001));

  // Update the vsync information
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(27500),
                                   base::TimeDelta::FromMicroseconds(10001));

  EXPECT_BEGIN_FRAME_USED(*obs_, 40000, 47502, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, 47502, 57503, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, 57503, 67504, 10001);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(60000));
}

TEST_F(DelayBasedBeginFrameSourceTest, AuthoritativeVSyncChanges) {
  task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(500),
                                   base::TimeDelta::FromMicroseconds(10000));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 500, 10500, 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, 10500, 20500, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 20500, 30500, 10000);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(20501));

  // This will keep the same timebase, so 500, 9999
  source_->SetAuthoritativeVSyncInterval(
      base::TimeDelta::FromMicroseconds(9999));
  EXPECT_BEGIN_FRAME_USED(*obs_, 30500, 40496, 9999);
  EXPECT_BEGIN_FRAME_USED(*obs_, 40496, 50495, 9999);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(40497));

  // Change the vsync params, but the new interval will be ignored.
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(400),
                                   base::TimeDelta::FromMicroseconds(1));
  EXPECT_BEGIN_FRAME_USED(*obs_, 50495, 60394, 9999);
  EXPECT_BEGIN_FRAME_USED(*obs_, 60394, 70393, 9999);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(60395));
}

TEST_F(DelayBasedBeginFrameSourceTest, MultipleObservers) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  // now_src_ starts off at 1000.
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs1, 10000, 20000, 10000);
  source_->AddObserver(&obs1);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, 20000, 30000, 10000);
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(10000));

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs2, 20000, 30000, 10000);
  source_->AddObserver(&obs2);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, 30000, 40000, 10000);
  EXPECT_BEGIN_FRAME_USED(obs2, 30000, 40000, 10000);
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(10000));

  source_->RemoveObserver(&obs1);

  EXPECT_BEGIN_FRAME_USED(obs2, 40000, 50000, 10000);
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(10000));

  source_->RemoveObserver(&obs2);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(50000));
  EXPECT_FALSE(task_runner_->HasPendingTasks());
}

TEST_F(DelayBasedBeginFrameSourceTest, DoubleTick) {
  StrictMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, 0, 10000, 10000);
  source_->AddObserver(&obs);

  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(5000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(4000));

  // No begin frame received.
  task_runner_->RunPendingTasks();

  // Begin frame received.
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(10000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(5000));
  EXPECT_BEGIN_FRAME_USED(obs, 10000, 20000, 10000);
  task_runner_->RunPendingTasks();
}

TEST_F(DelayBasedBeginFrameSourceTest, DoubleTickMissedFrame) {
  StrictMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, 0, 10000, 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(5000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(4000));

  // No missed frame received.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  // Missed frame received.
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(10000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(5000));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, 10000, 20000, 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);
}

}  // namespace
}  // namespace cc
