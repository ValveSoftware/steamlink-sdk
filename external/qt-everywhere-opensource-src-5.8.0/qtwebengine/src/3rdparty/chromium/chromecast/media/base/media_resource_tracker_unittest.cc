// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_resource_tracker.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

void RunUntilIdle(base::TaskRunner* task_runner) {
  base::WaitableEvent completion_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&base::WaitableEvent::Signal,
                                   base::Unretained(&completion_event)));
  completion_event.Wait();
}

// Collection of mocks to verify MediaResourceTracker takes the correct actions.
class MediaResourceTrackerTestMocks {
 public:
  MOCK_METHOD0(Initialize, void());  // CastMediaShlib::Initialize
  MOCK_METHOD0(Finalize, void());  // CastMediaShlib::Finalize
  MOCK_METHOD0(Destroyed, void());  // ~CastMediaResourceTracker
  MOCK_METHOD0(FinalizeCallback, void());  // callback to Finalize
};

class TestMediaResourceTracker : public MediaResourceTracker {
 public:
  TestMediaResourceTracker(
      MediaResourceTrackerTestMocks* test_mocks,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner)
      : MediaResourceTracker(ui_task_runner, media_task_runner),
        test_mocks_(test_mocks) {}
  ~TestMediaResourceTracker() override {
    EXPECT_TRUE(ui_task_runner_->BelongsToCurrentThread());
    test_mocks_->Destroyed();
  }

  void DoInitializeMediaLib() override {
    ASSERT_TRUE(media_task_runner_->BelongsToCurrentThread());
    test_mocks_->Initialize();
  }
  void DoFinalizeMediaLib() override {
    ASSERT_TRUE(media_task_runner_->BelongsToCurrentThread());
    test_mocks_->Finalize();
  }

 private:
  MediaResourceTrackerTestMocks* test_mocks_;
};

class MediaResourceTrackerTest : public ::testing::Test {
 public:
  MediaResourceTrackerTest() {}
  ~MediaResourceTrackerTest() override {}

 protected:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoop());
    media_thread_.reset(new base::Thread("MediaThread"));
    media_thread_->Start();
    media_task_runner_ = media_thread_->task_runner();

    test_mocks_.reset(new MediaResourceTrackerTestMocks());

    resource_tracker_ = new TestMediaResourceTracker(
        test_mocks_.get(), message_loop_->task_runner(),
        media_task_runner_);
  }

  void TearDown() override { media_thread_.reset(); }

  void IncrementMediaUsageCount() {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaResourceTracker::IncrementUsageCount,
                              base::Unretained(resource_tracker_)));
  }
  void DecrementMediaUsageCount() {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaResourceTracker::DecrementUsageCount,
                              base::Unretained(resource_tracker_)));
  }

  MediaResourceTracker* resource_tracker_;
  std::unique_ptr<MediaResourceTrackerTestMocks> test_mocks_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<base::Thread> media_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaResourceTrackerTest);
};

TEST_F(MediaResourceTrackerTest, BasicLifecycle) {
  // Startup and shutdown flow: Initialize then FinalizeAndDestroy
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);

  resource_tracker_->InitializeMediaLib();
  resource_tracker_->FinalizeAndDestroy();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, InitializeTwice) {
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);

  resource_tracker_->InitializeMediaLib();
  resource_tracker_->InitializeMediaLib();
  resource_tracker_->FinalizeAndDestroy();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, FinalizeWithoutInitialize) {
  EXPECT_CALL(*test_mocks_, Initialize()).Times(0);
  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);

  resource_tracker_->FinalizeAndDestroy();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, FinalizeResourceNotInUse) {
  // Check FinalizeCastMediaShlib works correctly with no users of
  // media resource.
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  resource_tracker_->InitializeMediaLib();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  EXPECT_CALL(*test_mocks_, FinalizeCallback()).Times(1);
  resource_tracker_->FinalizeMediaLib(
      base::Bind(&MediaResourceTrackerTestMocks::FinalizeCallback,
                 base::Unretained(test_mocks_.get())));
  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  resource_tracker_->FinalizeAndDestroy();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, FinalizeResourceInUse) {
  // Check FinalizeCastMediaShlib waits for resource to not be in use.
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  resource_tracker_->InitializeMediaLib();

  IncrementMediaUsageCount();
  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  resource_tracker_->FinalizeMediaLib(
      base::Bind(&MediaResourceTrackerTestMocks::FinalizeCallback,
                 base::Unretained(test_mocks_.get())));
  RunUntilIdle(media_task_runner_.get());

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  DecrementMediaUsageCount();

  EXPECT_CALL(*test_mocks_, FinalizeCallback()).Times(1);
  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  resource_tracker_->FinalizeAndDestroy();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, DestroyWaitForNoUsers) {
  // Check FinalizeAndDestroy waits for resource to not be in use.
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  resource_tracker_->InitializeMediaLib();

  IncrementMediaUsageCount();
  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  resource_tracker_->FinalizeAndDestroy();
  RunUntilIdle(media_task_runner_.get());

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  DecrementMediaUsageCount();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaResourceTrackerTest, DestroyWithPendingFinalize) {
  // Check finalize callback still made if FinalizeAndDestroy called
  // while waiting for resource usage to end.
  EXPECT_CALL(*test_mocks_, Initialize()).Times(1);
  resource_tracker_->InitializeMediaLib();

  IncrementMediaUsageCount();
  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*test_mocks_, Finalize()).Times(0);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(0);
  resource_tracker_->FinalizeMediaLib(
      base::Bind(&MediaResourceTrackerTestMocks::FinalizeCallback,
                 base::Unretained(test_mocks_.get())));
  resource_tracker_->FinalizeAndDestroy();
  RunUntilIdle(media_task_runner_.get());

  EXPECT_CALL(*test_mocks_, Finalize()).Times(1);
  EXPECT_CALL(*test_mocks_, Destroyed()).Times(1);
  EXPECT_CALL(*test_mocks_, FinalizeCallback()).Times(1);

  DecrementMediaUsageCount();

  RunUntilIdle(media_task_runner_.get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
}  // namespace chromecast
