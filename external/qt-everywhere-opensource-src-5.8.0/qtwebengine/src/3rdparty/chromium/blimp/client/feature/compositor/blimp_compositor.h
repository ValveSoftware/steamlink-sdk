// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_COMPOSITOR_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_COMPOSITOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/client/feature/compositor/blimp_input_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/remote_proto_channel.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace cc {
namespace proto {
class CompositorMessage;
class InitializeImpl;
}
class LayerTreeHost;
}

namespace blimp {

class BlimpMessage;

namespace client {

// The BlimpCompositorClient provides the BlimpCompositor with the necessary
// dependencies for cc::LayerTreeHost owned by this compositor and for
// communicating the compositor and input messages to the corresponding
// render widget of this compositor on the engine.
class BlimpCompositorClient {
 public:
  // These methods should provide the dependencies for cc::LayerTreeHost for
  // this compositor.
  virtual cc::LayerTreeSettings* GetLayerTreeSettings() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorTaskRunner() = 0;
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;
  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
  virtual cc::ImageSerializationProcessor* GetImageSerializationProcessor() = 0;
  virtual void DidCompleteSwapBuffers() = 0;
  virtual void DidCommitAndDrawFrame() = 0;

  // Should send web gesture events which could not be handled locally by the
  // compositor to the engine.
  virtual void SendWebGestureEvent(
      int render_widget_id,
      const blink::WebGestureEvent& gesture_event) = 0;

  // Should send the compositor messages from the remote client LayerTreeHost of
  // this compositor to the corresponding remote server LayerTreeHost.
  virtual void SendCompositorMessage(
      int render_widget_id,
      const cc::proto::CompositorMessage& message) = 0;

 protected:
  virtual ~BlimpCompositorClient() {}
};

// BlimpCompositor provides the basic framework and setup to host a
// LayerTreeHost.  The class that owns the LayerTreeHost is usually called the
// compositor, but the LayerTreeHost does the compositing work.  The rendering
// surface this compositor draws to is defined by the gfx::AcceleratedWidget set
// by SetAcceleratedWidget().  This class should only be accessed from the main
// thread.  Any interaction with the compositing thread should happen through
// the LayerTreeHost.
//
// The Blimp compositor owns the remote client cc::LayerTreeHost, which performs
// the compositing work for the remote server LayerTreeHost. The server
// LayerTreeHost for a BlimpCompositor is owned by the
// content::RenderWidgetCompositor. Thus, each BlimpCompositor is tied to a
// RenderWidget, identified by a custom |render_widget_id| generated on the
// engine. The lifetime of this compositor is controlled by its corresponding
// RenderWidget.
class BlimpCompositor
    : public cc::LayerTreeHostClient,
      public cc::RemoteProtoChannel,
      public BlimpInputManagerClient {
 public:
  BlimpCompositor(const int render_widget_id, BlimpCompositorClient* client);

  ~BlimpCompositor() override;

  // Sets whether or not this compositor actually draws to the output surface.
  // Setting this to false will make the compositor drop all of its resources
  // and the output surface.  Setting it to true again will rebuild the output
  // surface from the gfx::AcceleratedWidget (see SetAcceleratedWidget).
  // virtual for testing.
  virtual void SetVisible(bool visible);

  // Lets this compositor know that it can draw to |widget|.  This means that,
  // if this compositor is visible, it will build an output surface and GL
  // context around |widget| and will draw to it.  ReleaseAcceleratedWidget()
  // *must* be called before SetAcceleratedWidget() is called with the same
  // gfx::AcceleratedWidget on another compositor.
  // virtual for testing.
  virtual void SetAcceleratedWidget(gfx::AcceleratedWidget widget);

  // Releases the internally stored gfx::AcceleratedWidget and the associated
  // output surface.  This must be called before calling
  // SetAcceleratedWidget() with the same gfx::AcceleratedWidget on another
  // compositor.
  // virtual for testing.
  virtual void ReleaseAcceleratedWidget();

  // Forwards the touch event to the |input_manager_|.
  // virtual for testing.
  virtual bool OnTouchEvent(const ui::MotionEvent& motion_event);

  // Called to forward the compositor message from the remote server
  // LayerTreeHost of the render widget for this compositor.
  // virtual for testing.
  virtual void OnCompositorMessageReceived(
      std::unique_ptr<cc::proto::CompositorMessage> message);

  int render_widget_id() const { return render_widget_id_; }

 private:
  friend class BlimpCompositorForTesting;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void BeginMainFrame(const cc::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override;
  void DidCommit() override;
  void DidCommitAndDrawFrame() override;
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override;

  // RemoteProtoChannel implementation.
  void SetProtoReceiver(ProtoReceiver* receiver) override;
  void SendCompositorProto(const cc::proto::CompositorMessage& proto) override;

  // BlimpInputManagerClient implementation.
  void SendWebGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;

  // Internal method to correctly set the visibility on the |host_|. It will
  // make the |host_| visible if |visible| is true and we have a valid |window_|
  // If |visible_| is false, the host will also release its output surface.
  void SetVisibleInternal(bool visible);

  // Helper method to build the internal CC compositor instance from |message|.
  void CreateLayerTreeHost(
      const cc::proto::InitializeImpl& initialize_message);

  // Helper method to destroy the internal CC compositor instance and all its
  // associated state.
  void DestroyLayerTreeHost();

  // Creates (if necessary) and returns a TaskRunner for a thread meant to run
  // compositor rendering.
  void HandlePendingOutputSurfaceRequest();

  // The unique identifier for the render widget for this compositor.
  const int render_widget_id_;

  BlimpCompositorClient* client_;

  std::unique_ptr<cc::LayerTreeHost> host_;

  gfx::AcceleratedWidget window_;

  // Whether or not |host_| should be visible.  This is stored in case |host_|
  // is null when SetVisible() is called or if we don't have a
  // gfx::AcceleratedWidget to build an output surface from.
  bool host_should_be_visible_;

  // Whether there is an OutputSurface request pending from the current
  // |host_|. Becomes |true| if RequestNewOutputSurface is called, and |false|
  // if |host_| is deleted or we succeed in creating *and* initializing an
  // OutputSurface (which is essentially the contract with cc).
  bool output_surface_request_pending_;

  // To be notified of any incoming compositor protos that are specifically sent
  // to |render_widget_id_|.
  cc::RemoteProtoChannel::ProtoReceiver* remote_proto_channel_receiver_;

  // Handles input events for the current render widget. The lifetime of the
  // input manager is tied to the lifetime of the |host_| which owns the
  // cc::InputHandler. The input events are forwarded to this input handler by
  // the manager to be handled by the client compositor for the current render
  // widget.
  std::unique_ptr<BlimpInputManager> input_manager_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_COMPOSITOR_H_
