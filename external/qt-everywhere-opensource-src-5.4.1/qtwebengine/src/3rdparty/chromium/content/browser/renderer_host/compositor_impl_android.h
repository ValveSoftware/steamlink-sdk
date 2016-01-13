// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "content/browser/android/ui_resource_provider_impl.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/base/android/window_android_compositor.h"

class SkBitmap;
struct ANativeWindow;

namespace cc {
class Layer;
class LayerTreeHost;
}

namespace content {
class CompositorClient;
class UIResourceProvider;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient,
      public ImageTransportFactoryAndroidObserver,
      public ui::WindowAndroidCompositor {
 public:
  CompositorImpl(CompositorClient* client, gfx::NativeWindow root_window);
  virtual ~CompositorImpl();

  static bool IsInitialized();

  // Creates a surface texture and returns a surface texture id. Returns -1 on
  // failure.
  static int CreateSurfaceTexture(int child_process_id);

  // Destroy all surface textures associated with |child_process_id|.
  static void DestroyAllSurfaceTextures(int child_process_id);

 private:
  // Compositor implementation.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) OVERRIDE;
  virtual void SetWindowSurface(ANativeWindow* window) OVERRIDE;
  virtual void SetSurface(jobject surface) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void setDeviceScaleFactor(float factor) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Size& size) OVERRIDE;
  virtual void SetHasTransparentBackground(bool flag) OVERRIDE;
  virtual void SetNeedsComposite() OVERRIDE;
  virtual UIResourceProvider& GetUIResourceProvider() OVERRIDE;

  // LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE {}
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE {}
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE;
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE;

  // LayerTreeHostSingleThreadClient implementation.
  virtual void ScheduleComposite() OVERRIDE;
  virtual void ScheduleAnimation() OVERRIDE;
  virtual void DidPostSwapBuffers() OVERRIDE;
  virtual void DidAbortSwapBuffers() OVERRIDE;

  // ImageTransportFactoryAndroidObserver implementation.
  virtual void OnLostResources() OVERRIDE;

  // WindowAndroidCompositor implementation.
  virtual void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) OVERRIDE;
  virtual void RequestCopyOfOutputOnRootLayer(
      scoped_ptr<cc::CopyOutputRequest> request) OVERRIDE;
  virtual void OnVSync(base::TimeTicks frame_time,
                       base::TimeDelta vsync_period) OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE;

  enum CompositingTrigger {
    DO_NOT_COMPOSITE,
    COMPOSITE_IMMEDIATELY,
    COMPOSITE_EVENTUALLY,
  };
  void PostComposite(CompositingTrigger trigger);
  void Composite(CompositingTrigger trigger);

  bool WillCompositeThisFrame() const {
    return current_composite_task_ &&
           !current_composite_task_->callback().is_null();
  }
  bool DidCompositeThisFrame() const {
    return current_composite_task_ &&
           current_composite_task_->callback().is_null();
  }
  bool WillComposite() const {
    return WillCompositeThisFrame() ||
           composite_on_vsync_trigger_ != DO_NOT_COMPOSITE;
  }
  void CancelComposite() {
    DCHECK(WillComposite());
    if (WillCompositeThisFrame())
      current_composite_task_->Cancel();
    current_composite_task_.reset();
    composite_on_vsync_trigger_ = DO_NOT_COMPOSITE;
    will_composite_immediately_ = false;
  }
  cc::UIResourceId GenerateUIResourceFromUIResourceBitmap(
      const cc::UIResourceBitmap& bitmap,
      bool is_transient);
  void OnGpuChannelEstablished();

  // root_layer_ is the persistent internal root layer, while subroot_layer_
  // is the one attached by the compositor client.
  scoped_refptr<cc::Layer> root_layer_;
  scoped_refptr<cc::Layer> subroot_layer_;

  scoped_ptr<cc::LayerTreeHost> host_;
  content::UIResourceProviderImpl ui_resource_provider_;

  gfx::Size size_;
  bool has_transparent_background_;
  float device_scale_factor_;

  ANativeWindow* window_;
  int surface_id_;

  CompositorClient* client_;

  gfx::NativeWindow root_window_;

  // Used locally to track whether a call to LTH::Composite() did result in
  // a posted SwapBuffers().
  bool did_post_swapbuffers_;

  // Used locally to inhibit ScheduleComposite() during Layout().
  bool ignore_schedule_composite_;

  // Whether we need to composite in general because of any invalidation or
  // explicit request.
  bool needs_composite_;

  // Whether we need to update animations on the next composite.
  bool needs_animate_;

  // Whether we posted a task and are about to composite.
  bool will_composite_immediately_;

  // How we should schedule Composite during the next vsync.
  CompositingTrigger composite_on_vsync_trigger_;

  // The Composite operation scheduled for the current vsync interval.
  scoped_ptr<base::CancelableClosure> current_composite_task_;

  // The number of SwapBuffer calls that have not returned and ACK'd from
  // the GPU thread.
  unsigned int pending_swapbuffers_;

  base::TimeDelta vsync_period_;
  base::TimeTicks last_vsync_;

  base::WeakPtrFactory<CompositorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
