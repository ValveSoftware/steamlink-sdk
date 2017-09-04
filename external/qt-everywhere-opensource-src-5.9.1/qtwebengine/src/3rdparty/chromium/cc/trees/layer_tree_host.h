// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/input/browser_controls_state.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace cc {
class FrameSinkId;
class InputHandler;
class LayerTree;
class LayerTreeDebugState;
class LayerTreeMutator;
class LayerTreeSettings;
class CompositorFrameSink;
class SurfaceSequenceGenerator;
class SwapPromise;
class SwapPromiseManager;
class TaskRunnerProvider;
class UIResourceManager;

// This encapsulates the API for any embedder to use cc. The
// LayerTreeHostInProcess provides the implementation where the compositor
// thread components of this host run within the same process. Use
// LayerTreeHostInProcess::CreateThreaded/CreateSingleThread to get either.
class CC_EXPORT LayerTreeHost {
 public:
  virtual ~LayerTreeHost() {}

  // Returns the process global unique identifier for this LayerTreeHost.
  virtual int GetId() const = 0;

  // The current source frame number. This is incremented for each main frame
  // update(commit) pushed to the compositor thread.
  virtual int SourceFrameNumber() const = 0;

  // Returns the LayerTree that holds the main frame state pushed to the
  // LayerTreeImpl on commit.
  virtual LayerTree* GetLayerTree() = 0;
  virtual const LayerTree* GetLayerTree() const = 0;

  // Returns the UIResourceManager used to create UIResources for
  // UIResourceLayers pushed to the LayerTree.
  virtual UIResourceManager* GetUIResourceManager() const = 0;

  // Returns the TaskRunnerProvider used to access the main and compositor
  // thread task runners.
  virtual TaskRunnerProvider* GetTaskRunnerProvider() const = 0;

  // Returns the settings used by this host.
  virtual const LayerTreeSettings& GetSettings() const = 0;

  // Sets the client id used to generate the SurfaceId that uniquely identifies
  // the Surfaces produced by this compositor.
  virtual void SetFrameSinkId(const FrameSinkId& frame_sink_id) = 0;

  // Sets the LayerTreeMutator interface used to directly mutate the compositor
  // state on the compositor thread. (Compositor-Worker)
  virtual void SetLayerTreeMutator(
      std::unique_ptr<LayerTreeMutator> mutator) = 0;

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  virtual void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise) = 0;

  // Returns the SwapPromiseManager used to create SwapPromiseMonitors for this
  // host.
  virtual SwapPromiseManager* GetSwapPromiseManager() = 0;

  // Sets whether the content is suitable to use Gpu Rasterization.
  virtual void SetHasGpuRasterizationTrigger(bool has_trigger) = 0;

  // Visibility and CompositorFrameSink -------------------------------

  virtual void SetVisible(bool visible) = 0;
  virtual bool IsVisible() const = 0;

  // Called in response to an CompositorFrameSink request made to the client
  // using LayerTreeHostClient::RequestNewCompositorFrameSink. The client will
  // be informed of the CompositorFrameSink initialization status using
  // DidInitializaCompositorFrameSink or DidFailToInitializeCompositorFrameSink.
  // The request is completed when the host successfully initializes an
  // CompositorFrameSink.
  virtual void SetCompositorFrameSink(
      std::unique_ptr<CompositorFrameSink> compositor_frame_sink) = 0;

  // Forces the host to immediately release all references to the
  // CompositorFrameSink, if any. Can be safely called any time.
  virtual std::unique_ptr<CompositorFrameSink> ReleaseCompositorFrameSink() = 0;

  // Frame Scheduling (main and compositor frames) requests -------

  // Requests a main frame update even if no content has changed. This is used,
  // for instance in the case of RequestAnimationFrame from blink to ensure the
  // main frame update is run on the next tick without pre-emptively forcing a
  // full commit synchronization or layer updates.
  virtual void SetNeedsAnimate() = 0;

  // Requests a main frame update and also ensure that the host pulls layer
  // updates from the client, even if no content might have changed, without
  // forcing a full commit synchronization.
  virtual void SetNeedsUpdateLayers() = 0;

  // Requests that the next main frame update performs a full commit
  // synchronization.
  virtual void SetNeedsCommit() = 0;

  // Requests that the next frame re-chooses crisp raster scales for all layers.
  virtual void SetNeedsRecalculateRasterScales() = 0;

  // Returns true if a main frame (for any pipeline stage above) has been
  // requested.
  virtual bool BeginMainFrameRequested() const = 0;

  // Returns true if a main frame with commit synchronization has been
  // requested.
  virtual bool CommitRequested() const = 0;

  // Enables/disables the compositor from requesting main frame updates from the
  // client.
  virtual void SetDeferCommits(bool defer_commits) = 0;

  // Synchronously performs a main frame update and layer updates. Used only in
  // single threaded mode when the compositor's internal scheduling is disabled.
  virtual void LayoutAndUpdateLayers() = 0;

  // Synchronously performs a complete main frame update, commit and compositor
  // frame. Used only in single threaded mode when the compositor's internal
  // scheduling is disabled.
  virtual void Composite(base::TimeTicks frame_begin_time) = 0;

  // Requests a redraw (compositor frame) for the given rect.
  virtual void SetNeedsRedrawRect(const gfx::Rect& damage_rect) = 0;

  // Requests a main frame (including layer updates) and ensures that this main
  // frame results in a redraw for the complete viewport when producing the
  // CompositorFrame.
  virtual void SetNextCommitForcesRedraw() = 0;

  // Input Handling ---------------------------------------------

  // Notifies the compositor that input from the browser is being throttled till
  // the next commit. The compositor will prioritize activation of the pending
  // tree so a commit can be performed.
  virtual void NotifyInputThrottledUntilCommit() = 0;

  // Sets the state of the browser controls. (Used for URL bar animations on
  // android).
  virtual void UpdateBrowserControlsState(BrowserControlsState constraints,
                                          BrowserControlsState current,
                                          bool animate) = 0;

  // Returns a reference to the InputHandler used to respond to input events on
  // the compositor thread.
  virtual const base::WeakPtr<InputHandler>& GetInputHandler() const = 0;

  // Informs the compositor that an active fling gesture being processed on the
  // main thread has been finished.
  virtual void DidStopFlinging() = 0;

  // Debugging and benchmarks ---------------------------------
  virtual void SetDebugState(const LayerTreeDebugState& debug_state) = 0;
  virtual const LayerTreeDebugState& GetDebugState() const = 0;

  // Returns the id of the benchmark on success, 0 otherwise.
  virtual int ScheduleMicroBenchmark(
      const std::string& benchmark_name,
      std::unique_ptr<base::Value> value,
      const MicroBenchmark::DoneCallback& callback) = 0;

  // Returns true if the message was successfully delivered and handled.
  virtual bool SendMessageToMicroBenchmark(
      int id,
      std::unique_ptr<base::Value> value) = 0;

  // Methods used internally in cc. These are not intended to be a part of the
  // public API for use by the embedder ----------------------
  virtual SurfaceSequenceGenerator* GetSurfaceSequenceGenerator() = 0;

  // When the main thread informs the impl thread that it is ready to commit,
  // generally it would remain blocked till the main thread state is copied to
  // the pending tree. Calling this would ensure that the main thread remains
  // blocked till the pending tree is activated.
  virtual void SetNextCommitWaitsForActivation() = 0;

  // The LayerTreeHost tracks whether the content is suitable for Gpu raster.
  // Calling this will reset it back to not suitable state.
  virtual void ResetGpuRasterizationTracking() = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
