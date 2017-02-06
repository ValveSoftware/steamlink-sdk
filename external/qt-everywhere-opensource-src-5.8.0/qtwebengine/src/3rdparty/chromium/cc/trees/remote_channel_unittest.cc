// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/begin_frame_args_test.h"
#include "cc/test/layer_tree_test.h"

namespace cc {

class RemoteChannelTest : public LayerTreeTest {
 protected:
  RemoteChannelTest()
      : calls_received_(0), calls_received_on_both_server_and_client_(0) {}

  ~RemoteChannelTest() override {}

  void BeginTest() override {
    DCHECK(IsRemoteTest());
    BeginChannelTest();
  }
  virtual void BeginChannelTest() {}

  int calls_received_;

  // Since LayerTreeHost on engine and client share a common LayerTreeHostClient
  // for unit tests, there are some functions called twice. This variable keep
  // tracks of those function calls.
  int calls_received_on_both_server_and_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteChannelTest);
};

class RemoteChannelTestInitializationAndShutdown : public RemoteChannelTest {
  void SetVisibleOnImpl(bool visible) override { calls_received_++; }

  void RequestNewOutputSurface() override {
    LayerTreeTest::RequestNewOutputSurface();
    calls_received_++;
  }

  void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) override {
    calls_received_++;
  }

  void DidInitializeOutputSurface() override {
    calls_received_++;
    EndTest();
  }

  void FinishGLOnImpl() override { calls_received_++; }

  // On initialization and shutdown, each of the above calls should happen only
  // once.
  void AfterTest() override { EXPECT_EQ(5, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestInitializationAndShutdown);

class RemoteChannelTestMainThreadStoppedFlinging : public RemoteChannelTest {
  void BeginChannelTest() override { proxy()->MainThreadHasStoppedFlinging(); }

  void MainThreadHasStoppedFlingingOnImpl() override {
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestMainThreadStoppedFlinging);

class RemoteChannelTestDeferCommits : public RemoteChannelTest {
  void BeginChannelTest() override { DispatchSetDeferCommits(true); }

  void SetDeferCommitsOnImpl(bool defer_commits) override {
    EXPECT_TRUE(defer_commits);
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestDeferCommits);

class RemoteChannelTestNeedsRedraw : public RemoteChannelTest {
 public:
  RemoteChannelTestNeedsRedraw()
      : damaged_rect_(4, 5), device_viewport_size_(0, 0) {}

  void BeginChannelTest() override {
    device_viewport_size_ =
        gfx::Rect(remote_client_layer_tree_host()->device_viewport_size());
    layer_tree_host()->SetNeedsRedrawRect(damaged_rect_);
  }

  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) override {
    calls_received_++;
    if (calls_received_ == 1) {
      EXPECT_EQ(damaged_rect_, damage_rect);
    } else {
      // The second call is received after the output surface is successfully
      // initialized.
      EXPECT_EQ(device_viewport_size_, damage_rect);
      EndTest();
    }
  }

  void AfterTest() override { EXPECT_EQ(2, calls_received_); }

  gfx::Rect damaged_rect_;
  gfx::Rect device_viewport_size_;
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestNeedsRedraw);

class RemoteChannelTestReleaseOutputSurface : public RemoteChannelTest {
  void DidInitializeOutputSurface() override {
    SetVisibleOnLayerTreeHost(false);
    ReleaseOutputSurfaceOnLayerTreeHost();
  }

  void ReleaseOutputSurfaceOnImpl() override {
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestReleaseOutputSurface);

class RemoteChannelTestCommit : public RemoteChannelTest {
  void BeginChannelTest() override {
    layer_tree_host()->SetViewportSize(viewport_size_);
    PostSetNeedsCommitToMainThread();
  }

  void SetNeedsCommitOnImpl() override {
    calls_received_++;
    EXPECT_EQ(1, calls_received_);
  }

  void ReceivedBeginMainFrame() override {
    calls_received_++;
    EXPECT_EQ(2, calls_received_);
  }

  void StartCommitOnImpl() override {
    calls_received_++;
    EXPECT_EQ(3, calls_received_);
  }

  void DidCommitAndDrawFrame() override {
    calls_received_on_both_server_and_client_++;
  }

  void DidCompleteSwapBuffers() override {
    calls_received_on_both_server_and_client_++;
    if (calls_received_on_both_server_and_client_ == 4)
      EndTest();
  }

  void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // Ensure that we serialized and deserialized the LayerTreeHost for the
    // commit.
    EXPECT_EQ(viewport_size_, host_impl->device_viewport_size());
  }

  void AfterTest() override {
    EXPECT_EQ(3, calls_received_);
    EXPECT_EQ(4, calls_received_on_both_server_and_client_);
  }

  const gfx::Size viewport_size_ = gfx::Size(5, 3);
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestCommit);

class RemoteChannelTestBeginMainFrameAborted : public RemoteChannelTest {
  void BeginChannelTest() override { PostSetNeedsCommitToMainThread(); }

  void ScheduledActionWillSendBeginMainFrame() override {
    PostSetDeferCommitsToMainThread(true);
  }

  void BeginMainFrameAbortedOnImpl(CommitEarlyOutReason reason) override {
    EXPECT_EQ(reason, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestBeginMainFrameAborted);

class RemoteChannelTestReleaseOutputSurfaceDuringCommit
    : public RemoteChannelTest {
 public:
  RemoteChannelTestReleaseOutputSurfaceDuringCommit()
      : output_surface_initialized_count_(0), commit_count_(0) {}
  void BeginChannelTest() override { PostSetNeedsCommitToMainThread(); }

  void DidInitializeOutputSurface() override {
    ++output_surface_initialized_count_;

    switch (output_surface_initialized_count_) {
      case 1:
        // No commits can be performed before the first output surface is
        // initialized. The Scheduler should not send BeginMainFrames.
        EXPECT_EQ(0, commit_count_);
        break;
      case 2:
        // When the first BeginMainFrame is received on the server, we release
        // the output surface on the client. The RemoteChannelImpl on the client
        // will queue the commit received till a new output surface is
        // initialized, so we shouldn't see any commits till a second output
        // surface is provided to the LTH.
        EXPECT_EQ(0, commit_count_);
        break;
    }
  }

  void ReceivedBeginMainFrame() override {
    if (commit_count_ == 0) {
      // Release the output surface before we respond to the BeginMainFrame.
      // We perform the test only for the first BeginMainFrame request.
      SetVisibleOnLayerTreeHost(false);
      ReleaseOutputSurfaceOnLayerTreeHost();
      SetVisibleOnLayerTreeHost(true);
    }
  }

  void StartCommitOnImpl() override {
    ++commit_count_;
    if (commit_count_ == 1) {
      // If this is the first commit, then the output surface must have been
      // initialized twice.
      EXPECT_EQ(2, output_surface_initialized_count_);
      EndTest();
    }
  }

  void AfterTest() override {}

  // Accessed on the main thread and the impl thread, when the main thread is
  // blocked.
  int output_surface_initialized_count_;
  int commit_count_;
};

REMOTE_DIRECT_RENDERER_TEST_F(
    RemoteChannelTestReleaseOutputSurfaceDuringCommit);

}  // namespace cc
