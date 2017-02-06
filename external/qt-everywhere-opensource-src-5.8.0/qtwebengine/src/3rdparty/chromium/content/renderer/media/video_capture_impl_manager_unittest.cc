// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/child/child_process.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SaveArg;
using media::BindToCurrentLoop;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockVideoCaptureImpl : public VideoCaptureImpl {
 public:
  MockVideoCaptureImpl(media::VideoCaptureSessionId session_id,
                       VideoCaptureMessageFilter* filter,
                       base::Closure destruct_callback)
      : VideoCaptureImpl(session_id, filter),
        destruct_callback_(destruct_callback) {
  }

  ~MockVideoCaptureImpl() override { destruct_callback_.Run(); }

 private:
  base::Closure destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImpl);
};

class MockVideoCaptureImplManager : public VideoCaptureImplManager {
 public:
  explicit MockVideoCaptureImplManager(
      base::Closure destruct_video_capture_callback)
      : destruct_video_capture_callback_(
          destruct_video_capture_callback) {}
  ~MockVideoCaptureImplManager() override {}

 protected:
  VideoCaptureImpl* CreateVideoCaptureImplForTesting(
      media::VideoCaptureSessionId id,
      VideoCaptureMessageFilter* filter) const override {
    return new MockVideoCaptureImpl(id,
                                    filter,
                                    destruct_video_capture_callback_);
  }

 private:
  base::Closure destruct_video_capture_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImplManager);
};

class VideoCaptureImplManagerTest : public ::testing::Test {
 public:
  VideoCaptureImplManagerTest()
      : manager_(new MockVideoCaptureImplManager(
          BindToCurrentLoop(cleanup_run_loop_.QuitClosure()))) {
    params_.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);
    child_process_.reset(new ChildProcess());
  }

  void FakeChannelSetup() {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        child_process_->io_task_runner();
    if (!task_runner->BelongsToCurrentThread()) {
      task_runner->PostTask(
          FROM_HERE, base::Bind(&VideoCaptureImplManagerTest::FakeChannelSetup,
                                base::Unretained(this)));
      return;
    }
    manager_->video_capture_message_filter()->OnFilterAdded(NULL);
  }

 protected:
  MOCK_METHOD2(OnFrameReady,
               void(const scoped_refptr<media::VideoFrame>&,
                    base::TimeTicks estimated_capture_time));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());

  void OnStateUpdate(VideoCaptureState state) {
    switch (state) {
      case VIDEO_CAPTURE_STATE_STARTED:
        OnStarted();
        break;
      case VIDEO_CAPTURE_STATE_STOPPED:
        OnStopped();
        break;
      default:
        NOTREACHED();
    }
  }

  base::Closure StartCapture(const media::VideoCaptureParams& params) {
    return manager_->StartCapture(
        0, params, base::Bind(&VideoCaptureImplManagerTest::OnStateUpdate,
                              base::Unretained(this)),
        base::Bind(&VideoCaptureImplManagerTest::OnFrameReady,
                   base::Unretained(this)));
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  media::VideoCaptureParams params_;
  base::RunLoop cleanup_run_loop_;
  std::unique_ptr<MockVideoCaptureImplManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManagerTest);
};

// Multiple clients with the same session id. There is only one
// media::VideoCapture object.
TEST_F(VideoCaptureImplManagerTest, MultipleClients) {
  base::Closure release_cb1 = manager_->UseDevice(0);
  base::Closure release_cb2 = manager_->UseDevice(0);
  base::Closure stop_cb1, stop_cb2;
  {
    base::RunLoop run_loop;
    base::Closure quit_closure = BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnStarted()).WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*this, OnStarted()).RetiresOnSaturation();
    stop_cb1 = StartCapture(params_);
    stop_cb2 = StartCapture(params_);
    FakeChannelSetup();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = BindToCurrentLoop(run_loop.QuitClosure());
    EXPECT_CALL(*this, OnStopped()).WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*this, OnStopped()).RetiresOnSaturation();
    stop_cb1.Run();
    stop_cb2.Run();
    run_loop.Run();
  }

  release_cb1.Run();
  release_cb2.Run();
  cleanup_run_loop_.Run();
}

TEST_F(VideoCaptureImplManagerTest, NoLeak) {
  manager_->UseDevice(0).Reset();
  manager_.reset();
  cleanup_run_loop_.Run();
}

}  // namespace content
