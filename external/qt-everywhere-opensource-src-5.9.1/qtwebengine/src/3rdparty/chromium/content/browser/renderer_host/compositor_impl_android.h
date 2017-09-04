// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include <stddef.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/browser/android/compositor.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/android/context_provider_factory.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android_compositor.h"

struct ANativeWindow;

namespace cc {
class AnimationHost;
class Display;
class Layer;
class LayerTreeHost;
class OutputSurface;
class VulkanContextProvider;
}

namespace content {
class CompositorClient;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient,
      public ui::UIResourceProvider,
      public ui::WindowAndroidCompositor {
 public:
  class VSyncObserver {
   public:
    virtual void OnVSync(base::TimeTicks timebase,
                         base::TimeDelta interval) = 0;
  };

  CompositorImpl(CompositorClient* client, gfx::NativeWindow root_window);
  ~CompositorImpl() override;

  static bool IsInitialized();

  void AddObserver(VSyncObserver* observer);
  void RemoveObserver(VSyncObserver* observer);
  void OnNeedsBeginFramesChange(bool needs_begin_frames);

  // ui::ResourceProvider implementation.
  cc::UIResourceId CreateUIResource(cc::UIResourceClient* client) override;
  void DeleteUIResource(cc::UIResourceId resource_id) override;
  bool SupportsETC1NonPowerOfTwo() const override;

 private:
  // Compositor implementation.
  void SetRootLayer(scoped_refptr<cc::Layer> root) override;
  void SetSurface(jobject surface) override;
  void setDeviceScaleFactor(float factor) override;
  void SetWindowBounds(const gfx::Size& size) override;
  void SetHasTransparentBackground(bool flag) override;
  void SetNeedsComposite() override;
  ui::UIResourceProvider& GetUIResourceProvider() override;
  ui::ResourceManager& GetResourceManager() override;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewCompositorFrameSink() override;
  void DidInitializeCompositorFrameSink() override;
  void DidFailToInitializeCompositorFrameSink() override;
  void WillCommit() override {}
  void DidCommit() override;
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override;
  void DidCompletePageScaleAnimation() override {}

  // LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override;
  void DidLoseCompositorFrameSink() override;

  // WindowAndroidCompositor implementation.
  void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) override;
  void RequestCopyOfOutputOnRootLayer(
      std::unique_ptr<cc::CopyOutputRequest> request) override;
  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period) override;
  void SetNeedsAnimate() override;
  cc::FrameSinkId GetFrameSinkId() override;

  void SetVisible(bool visible);
  void CreateLayerTreeHost();

  void HandlePendingCompositorFrameSinkRequest();

#if defined(ENABLE_VULKAN)
  void CreateVulkanOutputSurface();
#endif
  void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      ui::ContextProviderFactory::GpuChannelHostResult result);
  void InitializeDisplay(
      std::unique_ptr<cc::OutputSurface> display_output_surface,
      scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
      scoped_refptr<cc::ContextProvider> context_provider);

  bool HavePendingReadbacks();
  void SetBackgroundColor(int color);

  cc::FrameSinkId frame_sink_id_;

  // root_layer_ is the persistent internal root layer, while subroot_layer_
  // is the one attached by the compositor client.
  scoped_refptr<cc::Layer> subroot_layer_;

  // Subtree for hidden layers with CopyOutputRequests on them.
  scoped_refptr<cc::Layer> readback_layer_tree_;

  // Destruction order matters here:
  base::ObserverList<VSyncObserver, true> observer_list_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> host_;
  ui::ResourceManagerImpl resource_manager_;

  std::unique_ptr<cc::Display> display_;

  gfx::Size size_;
  bool has_transparent_background_;
  float device_scale_factor_;

  ANativeWindow* window_;
  gpu::SurfaceHandle surface_handle_;

  CompositorClient* client_;

  gfx::NativeWindow root_window_;

  // Whether we need to update animations on the next composite.
  bool needs_animate_;

  // The number of SwapBuffer calls that have not returned and ACK'd from
  // the GPU thread.
  unsigned int pending_swapbuffers_;

  size_t num_successive_context_creation_failures_;

  // Whether there is an CompositorFrameSink request pending from the current
  // |host_|. Becomes |true| if RequestNewCompositorFrameSink is called, and
  // |false| if |host_| is deleted or we succeed in creating *and* initializing
  // a CompositorFrameSink (which is essentially the contract with cc).
  bool compositor_frame_sink_request_pending_;

  gpu::Capabilities gpu_capabilities_;
  bool needs_begin_frames_;
  base::WeakPtrFactory<CompositorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
