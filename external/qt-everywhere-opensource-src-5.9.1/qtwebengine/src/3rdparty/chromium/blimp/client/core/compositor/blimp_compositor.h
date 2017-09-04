// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_H_

#include <content/public/renderer/remote_proto_channel.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "blimp/client/core/compositor/blimp_compositor_frame_sink_proxy.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace cc {
class AnimationHost;
class InputHandler;

namespace proto {
class CompositorMessage;
}  // namespace proto

class ContextProvider;
class CopyOutputRequest;
class Layer;
class LayerTreeHostInProcess;
class LayerTreeSettings;
class LocalFrameid;
class Surface;
class SurfaceFactory;
class SurfaceIdAllocator;
}  // namespace cc

namespace blimp {
class BlimpMessage;

namespace client {

class BlimpCompositorDependencies;

// The BlimpCompositorClient provides the BlimpCompositor with the necessary
// dependencies for cc::LayerTreeHost owned by this compositor and for
// communicating the compositor and input messages to the corresponding
// render widget of this compositor on the engine.
class BlimpCompositorClient {
 public:
  // Should send the compositor messages from the remote client LayerTreeHost of
  // this compositor to the corresponding remote server LayerTreeHost.
  virtual void SendCompositorMessage(
      const cc::proto::CompositorMessage& message) = 0;

 protected:
  virtual ~BlimpCompositorClient() {}
};

// BlimpCompositor provides the basic framework and setup to host a
// LayerTreeHost. This class owns the remote client cc::LayerTreeHost, which
// performs the compositing work for the remote server LayerTreeHost. The server
// LayerTreeHost for a BlimpCompositor is owned by the
// content::RenderWidgetCompositor. Thus, each BlimpCompositor is tied to a
// RenderWidget, identified by a custom |render_widget_id| generated on the
// engine. The lifetime of this compositor is controlled by its corresponding
// RenderWidget.
// This class should only be accessed from the main thread.
class BlimpCompositor : public cc::LayerTreeHostClient,
                        public BlimpCompositorFrameSinkProxy,
                        public cc::SurfaceFactoryClient,
                        public cc::CompositorStateDeserializerClient {
 public:
  static std::unique_ptr<BlimpCompositor> Create(
      BlimpCompositorDependencies* compositor_dependencies,
      BlimpCompositorClient* client);

  ~BlimpCompositor() override;

  void SetVisible(bool visible);
  bool IsVisible() const;

  // Requests a copy of the compositor frame.
  // Setting |flush_pending_update| to true ensures that if a frame update on
  // the main thread is pending, then it is drawn before copying from the
  // surface.
  void RequestCopyOfOutput(std::unique_ptr<cc::CopyOutputRequest> copy_request,
                           bool flush_pending_update);

  // Called to forward the compositor message from the remote server
  // LayerTreeHost of the render widget for this compositor.
  // virtual for testing.
  void OnCompositorMessageReceived(
      std::unique_ptr<cc::proto::CompositorMessage> message);

  scoped_refptr<cc::Layer> layer() const { return layer_; }

  // Returns a reference to the InputHandler used to respond to input events on
  // the compositor thread.
  const base::WeakPtr<cc::InputHandler>& GetInputHandler();

  cc::CompositorStateDeserializer* compositor_state_deserializer_for_testing() {
    return compositor_state_deserializer_.get();
  }

  bool HasPendingFrameUpdateFromEngine() const;

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
                           float top_controls_delta) override;
  void RequestNewCompositorFrameSink() override;
  void DidInitializeCompositorFrameSink() override;
  // TODO(khushalsagar): Need to handle context initialization failures.
  void DidFailToInitializeCompositorFrameSink() override {}
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override;
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override {}

  // CompositorStateDeserializerClient implementation.
  void DidUpdateLocalState() override;

 protected:
  BlimpCompositor(BlimpCompositorDependencies* compositor_dependencies,
                  BlimpCompositorClient* client);

  void Initialize();
  virtual std::unique_ptr<cc::LayerTreeHostInProcess> CreateLayerTreeHost();

  cc::AnimationHost* animation_host() { return animation_host_.get(); }

 private:
  class FrameTrackingSwapPromise;

  // BlimpCompositorFrameSinkProxy implementation.
  void BindToProxyClient(
      base::WeakPtr<BlimpCompositorFrameSinkProxyClient> proxy_client) override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void UnbindProxyClient() override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override {}

  // Called when the a ContextProvider has been created by the
  // CompositorDependencies class.  If |host_| is waiting on an
  // CompositorFrameSink this will build one for it.
  void OnContextProvidersCreated(
      const scoped_refptr<cc::ContextProvider>& compositor_context_provider,
      const scoped_refptr<cc::ContextProvider>& worker_context_provider);

  // Helper method to get the embedder dependencies.
  CompositorDependencies* GetEmbedderDeps();

  // TODO(khushalsagar): Move all of this to the |DocumentView| or another
  // platform specific class. So we use the DelegatedFrameHostAndroid like the
  // RenderWidgetHostViewAndroid.
  void DestroyDelegatedContent();

  // Helper method to destroy the internal CC LayerTreeHost instance and all its
  // associated state.
  void DestroyLayerTreeHost();

  // Acks a submitted CompositorFrame when it has been processed and another
  // frame should be started.
  void SubmitCompositorFrameAck();

  // Sends an update to the engine if the state on the client was modified and
  // an ack for a previous update sent is not pending.
  void FlushClientState();

  void MakeCopyRequestOnNextSwap(
      std::unique_ptr<cc::CopyOutputRequest> copy_request);

  BlimpCompositorClient* client_;

  BlimpCompositorDependencies* compositor_dependencies_;

  cc::FrameSinkId frame_sink_id_;

  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHostInProcess> host_;

  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  base::WeakPtr<BlimpCompositorFrameSinkProxyClient> proxy_client_;
  bool bound_to_proxy_;

  // Whether or not |host_| has asked for a new CompositorFrameSink.
  bool compositor_frame_sink_request_pending_;

  // Data for the current frame.
  cc::LocalFrameId local_frame_id_;
  gfx::Size current_surface_size_;

  base::ThreadChecker thread_checker_;

  // Surfaces related stuff and layer which holds the delegated content from the
  // compositor.
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  scoped_refptr<cc::Layer> layer_;

  std::vector<std::unique_ptr<cc::CopyOutputRequest>>
      copy_requests_for_next_swap_;

  // Stores a frame update received from the engine, when a threaded
  // LayerTreeHost is used. There can only be a single frame in flight at any
  // point.
  std::unique_ptr<cc::proto::CompositorMessage> pending_frame_update_;

  // Used with a threaded LayerTreeHost to deserialize proto updates from the
  // engine into the LayerTree.
  std::unique_ptr<cc::CompositorStateDeserializer>
      compositor_state_deserializer_;

  // Set to true if the compositor state on the client was modified on the impl
  // thread and an update needs to be sent to the engine.
  bool client_state_dirty_ = false;

  // Set to true if a client state update was sent to the engine and an ack for
  // this update from the engine is pending.
  bool client_state_update_ack_pending_ = false;

  base::WeakPtrFactory<BlimpCompositor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_H_
