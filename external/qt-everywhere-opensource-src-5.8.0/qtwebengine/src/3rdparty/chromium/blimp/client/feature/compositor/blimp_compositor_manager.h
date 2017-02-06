// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_COMPOSITOR_MANAGER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_COMPOSITOR_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "blimp/client/feature/compositor/blimp_compositor.h"
#include "blimp/client/feature/compositor/blimp_gpu_memory_buffer_manager.h"
#include "blimp/client/feature/compositor/blob_image_serialization_processor.h"
#include "blimp/client/feature/render_widget_feature.h"
#include "cc/trees/layer_tree_settings.h"

namespace blimp {
namespace client {

class BlimpCompositorManagerClient {
 public:
  virtual void OnSwapBuffersCompleted() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
};

// The BlimpCompositorManager manages multiple BlimpCompositor instances, each
// mapped to a render widget on the engine. The compositor corresponding to
// the render widget initialized on the engine will be the |active_compositor_|.
// Only the |active_compositor_| holds the accelerated widget and builds the
// output surface from this widget to draw to the view. All events from the
// native view are forwarded to this compositor.
class BlimpCompositorManager
    : public RenderWidgetFeature::RenderWidgetFeatureDelegate,
      public BlimpCompositorClient {
 public:
  explicit BlimpCompositorManager(RenderWidgetFeature* render_widget_feature,
                                  BlimpCompositorManagerClient* client);
  ~BlimpCompositorManager() override;

  void SetVisible(bool visible);

  void SetAcceleratedWidget(gfx::AcceleratedWidget widget);

  void ReleaseAcceleratedWidget();

  bool OnTouchEvent(const ui::MotionEvent& motion_event);

 protected:
  // Populates the cc::LayerTreeSettings used by the cc::LayerTreeHost of the
  // BlimpCompositors created by this manager. Can be overridden to provide
  // custom settings parameters.
  virtual void GenerateLayerTreeSettings(cc::LayerTreeSettings* settings);

  // virtual for testing.
  virtual std::unique_ptr<BlimpCompositor> CreateBlimpCompositor(
      int render_widget_id,
      BlimpCompositorClient* client);

  // Returns the compositor for the |render_widget_id|. Will return nullptr if
  // no compositor is found.
  // protected for testing.
  BlimpCompositor* GetCompositor(int render_widget_id);

 private:
  // RenderWidgetFeatureDelegate implementation.
  void OnRenderWidgetCreated(int render_widget_id) override;
  void OnRenderWidgetInitialized(int render_widget_id) override;
  void OnRenderWidgetDeleted(int render_widget_id) override;
  void OnCompositorMessageReceived(
      int render_widget_id,
      std::unique_ptr<cc::proto::CompositorMessage> message) override;

  // BlimpCompositorClient implementation.
  void DidCompleteSwapBuffers() override;
  void DidCommitAndDrawFrame() override;
  cc::LayerTreeSettings* GetLayerTreeSettings() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorTaskRunner() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::ImageSerializationProcessor* GetImageSerializationProcessor() override;
  void SendWebGestureEvent(
      int render_widget_id,
      const blink::WebGestureEvent& gesture_event) override;
  void SendCompositorMessage(
      int render_widget_id,
      const cc::proto::CompositorMessage& message) override;

  bool visible_;

  gfx::AcceleratedWidget window_;

  std::unique_ptr<cc::LayerTreeSettings> settings_;

  std::unique_ptr<BlimpGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  // A map of render_widget_ids to the BlimpCompositor instance.
  typedef std::map<int, std::unique_ptr<BlimpCompositor>> CompositorMap;
  CompositorMap compositors_;

  // The |active_compositor_| represents the compositor from the CompositorMap
  // that is currently visible and has the |window_|. It corresponds to the
  // render widget currently initialized on the engine.
  BlimpCompositor* active_compositor_;

  // Lazily created thread that will run the compositor rendering tasks and will
  // be shared by all compositor instances.
  std::unique_ptr<base::Thread> compositor_thread_;

  // The bridge to the network layer that does the proto/RenderWidget id work.
  // BlimpCompositorManager does not own this and it is expected to outlive this
  // BlimpCompositorManager instance.
  RenderWidgetFeature* render_widget_feature_;
  BlimpCompositorManagerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositorManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_COMPOSITOR_MANAGER_H_
