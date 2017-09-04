// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_MAIN_H_
#define CC_TREES_PROXY_MAIN_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/channel_main.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_common.h"

namespace cc {

class MutatorEvents;
class ChannelMain;
class CompositorFrameSink;
class LayerTreeHostInProcess;
class LayerTreeMutator;

// This class aggregates all interactions that the impl side of the compositor
// needs to have with the main side.
// The class is created and lives on the main thread.
class CC_EXPORT ProxyMain : public Proxy {
 public:
  static std::unique_ptr<ProxyMain> CreateThreaded(
      LayerTreeHostInProcess* layer_tree_host,
      TaskRunnerProvider* task_runner_provider);

  ~ProxyMain() override;

  // Commits between the main and impl threads are processed through a pipeline
  // with the following stages. For efficiency we can early out at any stage if
  // we decide that no further processing is necessary.
  enum CommitPipelineStage {
    NO_PIPELINE_STAGE,
    ANIMATE_PIPELINE_STAGE,
    UPDATE_LAYERS_PIPELINE_STAGE,
    COMMIT_PIPELINE_STAGE,
  };

  void DidReceiveCompositorFrameAck();
  void BeginMainFrameNotExpectedSoon();
  void DidCommitAndDrawFrame();
  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events);
  void DidLoseCompositorFrameSink();
  void RequestNewCompositorFrameSink();
  void DidInitializeCompositorFrameSink(bool success);
  void DidCompletePageScaleAnimation();
  void BeginMainFrame(
      std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state);

  ChannelMain* channel_main() const { return channel_main_.get(); }
  CommitPipelineStage max_requested_pipeline_stage() const {
    return max_requested_pipeline_stage_;
  }
  CommitPipelineStage current_pipeline_stage() const {
    return current_pipeline_stage_;
  }
  CommitPipelineStage final_pipeline_stage() const {
    return final_pipeline_stage_;
  }

 protected:
  ProxyMain(LayerTreeHostInProcess* layer_tree_host,
            TaskRunnerProvider* task_runner_provider);

 private:
  friend class ProxyMainForTest;

  // Proxy implementation.
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetCompositorFrameSink(
      CompositorFrameSink* compositor_frame_sink) override;
  void SetVisible(bool visible) override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetNextCommitWaitsForActivation() override;
  void NotifyInputThrottledUntilCommit() override;
  void SetDeferCommits(bool defer_commits) override;
  bool CommitRequested() const override;
  bool BeginMainFrameRequested() const override;
  void MainThreadHasStoppedFlinging() override;
  void Start() override;
  void Stop() override;
  bool SupportsImplScrolling() const override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  bool MainFrameWillHappenForTesting() override;
  void ReleaseCompositorFrameSink() override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;

  // This sets the channel used by ProxyMain to communicate with ProxyImpl.
  void SetChannel(std::unique_ptr<ChannelMain> channel_main);

  // Returns |true| if the request was actually sent, |false| if one was
  // already outstanding.
  bool SendCommitRequestToImplThreadIfNeeded(
      CommitPipelineStage required_stage);
  bool IsMainThread() const;

  LayerTreeHostInProcess* layer_tree_host_;

  TaskRunnerProvider* task_runner_provider_;

  const int layer_tree_host_id_;

  // The furthest pipeline stage which has been requested for the next
  // commit.
  CommitPipelineStage max_requested_pipeline_stage_;
  // The commit pipeline stage that is currently being processed.
  CommitPipelineStage current_pipeline_stage_;
  // The commit pipeline stage at which processing for the current commit
  // will stop. Only valid while we are executing the pipeline (i.e.,
  // |current_pipeline_stage| is set to a pipeline stage).
  CommitPipelineStage final_pipeline_stage_;

  bool commit_waits_for_activation_;

  // Set when the Proxy is started using Proxy::Start() and reset when it is
  // stopped using Proxy::Stop().
  bool started_;

  bool defer_commits_;

  std::unique_ptr<ChannelMain> channel_main_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMain);
};

}  // namespace cc

#endif  // CC_TREES_PROXY_MAIN_H_
