// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"

#define PROXY_MAIN_THREADED_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, MultiThread) { Run(true); }

// Do common tests for single thread proxy and proxy main in threaded mode.
// TODO(simonhong): Add SINGLE_THREAD_PROXY_TEST_F
#define PROXY_TEST_SCHEDULED_ACTION(TEST_FIXTURE_NAME) \
  PROXY_MAIN_THREADED_TEST_F(TEST_FIXTURE_NAME);

namespace cc {

class ProxyTest : public LayerTreeTest {
 protected:
  ProxyTest() {}
  ~ProxyTest() override {}

  void Run(bool threaded) {
    // We don't need to care about delegating mode.
    bool delegating_renderer = true;

    CompositorMode mode =
        threaded ? CompositorMode::THREADED : CompositorMode::SINGLE_THREADED;
    RunTest(mode, delegating_renderer);
  }

  void BeginTest() override {}
  void AfterTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyTest);
};

class ProxyTestScheduledActionsBasic : public ProxyTest {
 protected:
  void BeginTest() override { proxy()->SetNeedsCommit(); }

  void ScheduledActionBeginOutputSurfaceCreation() override {
    EXPECT_EQ(0, action_phase_++);
  }

  void ScheduledActionSendBeginMainFrame() override {
    EXPECT_EQ(1, action_phase_++);
  }

  void ScheduledActionCommit() override { EXPECT_EQ(2, action_phase_++); }

  void ScheduledActionDrawAndSwapIfPossible() override {
    EXPECT_EQ(3, action_phase_++);
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(4, action_phase_); }

  ProxyTestScheduledActionsBasic() : action_phase_(0) {
  }
  ~ProxyTestScheduledActionsBasic() override {}

 private:
  int action_phase_;

  DISALLOW_COPY_AND_ASSIGN(ProxyTestScheduledActionsBasic);
};

PROXY_TEST_SCHEDULED_ACTION(ProxyTestScheduledActionsBasic);

class ProxyMainThreaded : public ProxyTest {
 protected:
  ProxyMainThreaded()
      : update_check_layer_(FakePictureLayer::Create(&client_)) {}
  ~ProxyMainThreaded() override {}

  void SetupTree() override {
    layer_tree_host()->SetRootLayer(update_check_layer_);
    ProxyTest::SetupTree();
    client_.set_bounds(update_check_layer_->bounds());
  }

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> update_check_layer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreaded);
};

class ProxyMainThreadedSetNeedsCommit : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsCommit() {}
  ~ProxyMainThreadedSetNeedsCommit() override {}

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());

    proxy()->SetNeedsCommit();

    EXPECT_EQ(ProxyMain::COMMIT_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsCommit);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsCommit);

class ProxyMainThreadedSetNeedsAnimate : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsAnimate() {}
  ~ProxyMainThreadedSetNeedsAnimate() override {}

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());

    proxy()->SetNeedsAnimate();

    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(0, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsAnimate);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsAnimate);

class ProxyMainThreadedSetNeedsUpdateLayers : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsUpdateLayers() {}
  ~ProxyMainThreadedSetNeedsUpdateLayers() override {}

  void BeginTest() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());

    proxy()->SetNeedsUpdateLayers();

    EXPECT_EQ(ProxyMain::UPDATE_LAYERS_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsUpdateLayers);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsUpdateLayers);

class ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating
    : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating() {}
  ~ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating() override {}

  void BeginTest() override { proxy()->SetNeedsAnimate(); }

  void WillBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());

    proxy()->SetNeedsUpdateLayers();

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::UPDATE_LAYERS_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsUpdateLayersWhileAnimating);

class ProxyMainThreadedSetNeedsCommitWhileAnimating : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedSetNeedsCommitWhileAnimating() {}
  ~ProxyMainThreadedSetNeedsCommitWhileAnimating() override {}

  void BeginTest() override { proxy()->SetNeedsAnimate(); }

  void WillBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
    EXPECT_EQ(ProxyMain::ANIMATE_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());

    proxy()->SetNeedsCommit();

    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::COMMIT_PIPELINE_STAGE,
              GetProxyMainForTest()->final_pipeline_stage());
  }

  void DidBeginMainFrame() override {
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->max_requested_pipeline_stage());
    EXPECT_EQ(ProxyMain::NO_PIPELINE_STAGE,
              GetProxyMainForTest()->current_pipeline_stage());
  }

  void DidCommit() override {
    EXPECT_EQ(1, update_check_layer_->update_count());
    EndTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedSetNeedsCommitWhileAnimating);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedSetNeedsCommitWhileAnimating);

class ProxyMainThreadedCommitWaitsForActivation : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedCommitWaitsForActivation() : num_commits_(0) {}
  ~ProxyMainThreadedCommitWaitsForActivation() override {}

  void BeginTest() override { proxy()->SetNeedsCommit(); }

  void ScheduledActionCommit() override {
    switch (num_commits_) {
      case 0:
        // Set next commit waits for activation and start another commit.
        PostNextCommitWaitsForActivationToMainThread();
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
    }
    num_commits_++;
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    CompletionEvent* activation_completion_event =
        GetProxyImplForTest()->ActivationCompletionEventForTesting();
    switch (num_commits_) {
      case 1:
        EXPECT_FALSE(activation_completion_event);
        break;
      case 2:
        EXPECT_TRUE(activation_completion_event);
        EXPECT_FALSE(activation_completion_event->IsSignaled());
        break;
      case 3:
        EXPECT_FALSE(activation_completion_event);
        EndTest();
        break;
    }
  }

  void AfterTest() override {
    // It is safe to read num_commits_ on the main thread now since AfterTest()
    // runs after the LayerTreeHost is destroyed and the impl thread tear down
    // is finished.
    EXPECT_EQ(3, num_commits_);
  }

 private:
  int num_commits_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedCommitWaitsForActivation);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedCommitWaitsForActivation);

// Test for a corner case of main frame before activation (MFBA) and commit
// waits for activation. If a commit (with wait for activation flag set)
// is ready before the activation for a previous commit then the activation
// should not signal the completion event of the second commit.
class ProxyMainThreadedCommitWaitsForActivationMFBA : public ProxyMainThreaded {
 protected:
  ProxyMainThreadedCommitWaitsForActivationMFBA() : num_commits_(0) {}
  ~ProxyMainThreadedCommitWaitsForActivationMFBA() override {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->main_frame_before_activation_enabled = true;
    ProxyMainThreaded::InitializeSettings(settings);
  }

  void BeginTest() override { proxy()->SetNeedsCommit(); }

  // This is called right before NotifyReadyToCommit.
  void StartCommitOnImpl() override {
    switch (num_commits_) {
      case 0:
        // Block activation until next commit is ready.
        GetProxyImplForTest()->BlockNotifyReadyToActivateForTesting(true);
        break;
      case 1:
        // Unblock activation of first commit after second commit is ready.
        ImplThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&ProxyImplForTest::BlockNotifyReadyToActivateForTesting,
                       base::Unretained(GetProxyImplForTest()), false));
        break;
    }
  }

  void ScheduledActionCommit() override {
    switch (num_commits_) {
      case 0:
        // Set next commit waits for activation and start another commit.
        PostNextCommitWaitsForActivationToMainThread();
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
    }
    num_commits_++;
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    CompletionEvent* activation_completion_event =
        GetProxyImplForTest()->ActivationCompletionEventForTesting();
    switch (num_commits_) {
      case 1:
        EXPECT_FALSE(activation_completion_event);
        break;
      case 2:
        EXPECT_TRUE(activation_completion_event);
        EXPECT_FALSE(activation_completion_event->IsSignaled());
        break;
      case 3:
        EXPECT_FALSE(activation_completion_event);
        EndTest();
        break;
    }
  }

  void AfterTest() override {
    // It is safe to read num_commits_ on the main thread now since AfterTest()
    // runs after the LayerTreeHost is destroyed and the impl thread tear down
    // is finished.
    EXPECT_EQ(3, num_commits_);
  }

 private:
  int num_commits_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMainThreadedCommitWaitsForActivationMFBA);
};

PROXY_MAIN_THREADED_TEST_F(ProxyMainThreadedCommitWaitsForActivationMFBA);

}  // namespace cc
