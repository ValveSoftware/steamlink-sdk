// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/feature/compositor/blimp_context_provider.h"
#include "blimp/client/feature/compositor/blimp_output_surface.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/output/output_surface.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "net/base/net_errors.h"
#include "ui/gl/gl_surface.h"

namespace blimp {
namespace client {

BlimpCompositor::BlimpCompositor(int render_widget_id,
                                 BlimpCompositorClient* client)
    : render_widget_id_(render_widget_id),
      client_(client),
      window_(gfx::kNullAcceleratedWidget),
      host_should_be_visible_(false),
      output_surface_request_pending_(false),
      remote_proto_channel_receiver_(nullptr) {}

BlimpCompositor::~BlimpCompositor() {
  if (host_)
    DestroyLayerTreeHost();
}

void BlimpCompositor::SetVisible(bool visible) {
  host_should_be_visible_ = visible;
  SetVisibleInternal(host_should_be_visible_);
}

void BlimpCompositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  if (widget == window_)
    return;

  DCHECK(window_ == gfx::kNullAcceleratedWidget);
  window_ = widget;

  // The compositor should not be visible if there is no output surface.
  DCHECK(!host_ || !host_->visible());

  // This will properly set visibility and will build the output surface if
  // necessary.
  SetVisibleInternal(host_should_be_visible_);
}

void BlimpCompositor::ReleaseAcceleratedWidget() {
  if (window_ == gfx::kNullAcceleratedWidget)
    return;

  // Hide the compositor and drop the output surface if necessary.
  SetVisibleInternal(false);

  window_ = gfx::kNullAcceleratedWidget;
}

bool BlimpCompositor::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (input_manager_)
    return input_manager_->OnTouchEvent(motion_event);
  return false;
}

void BlimpCompositor::WillBeginMainFrame() {}

void BlimpCompositor::DidBeginMainFrame() {}

void BlimpCompositor::BeginMainFrame(const cc::BeginFrameArgs& args) {}

void BlimpCompositor::BeginMainFrameNotExpectedSoon() {}

void BlimpCompositor::UpdateLayerTreeHost() {}

void BlimpCompositor::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {}

void BlimpCompositor::RequestNewOutputSurface() {
  output_surface_request_pending_ = true;
  HandlePendingOutputSurfaceRequest();
}

void BlimpCompositor::DidInitializeOutputSurface() {
}

void BlimpCompositor::DidFailToInitializeOutputSurface() {}

void BlimpCompositor::WillCommit() {}

void BlimpCompositor::DidCommit() {}

void BlimpCompositor::DidCommitAndDrawFrame() {
  client_->DidCommitAndDrawFrame();
}

void BlimpCompositor::DidCompleteSwapBuffers() {
  client_->DidCompleteSwapBuffers();
}

void BlimpCompositor::DidCompletePageScaleAnimation() {}

void BlimpCompositor::SetProtoReceiver(ProtoReceiver* receiver) {
  remote_proto_channel_receiver_ = receiver;
}

void BlimpCompositor::SendCompositorProto(
    const cc::proto::CompositorMessage& proto) {
  client_->SendCompositorMessage(render_widget_id_, proto);
}

void BlimpCompositor::OnCompositorMessageReceived(
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  DCHECK(message->has_to_impl());
  const cc::proto::CompositorMessageToImpl& to_impl_proto =
      message->to_impl();

  DCHECK(to_impl_proto.has_message_type());
  switch (to_impl_proto.message_type()) {
    case cc::proto::CompositorMessageToImpl::UNKNOWN:
      NOTIMPLEMENTED() << "Ignoring message of UNKNOWN type";
      break;
    case cc::proto::CompositorMessageToImpl::INITIALIZE_IMPL:
      DCHECK(!host_);
      DCHECK(to_impl_proto.has_initialize_impl_message());

      // Create the remote client LayerTreeHost for the compositor.
      CreateLayerTreeHost(to_impl_proto.initialize_impl_message());
      break;
    case cc::proto::CompositorMessageToImpl::CLOSE_IMPL:
      DCHECK(host_);

      // Destroy the remote client LayerTreeHost for the compositor.
      DestroyLayerTreeHost();
      break;
    default:
      // We should have a receiver if we're getting compositor messages that
      // are not INITIALIZE_IMPL or CLOSE_IMPL.
      DCHECK(remote_proto_channel_receiver_);
      remote_proto_channel_receiver_->OnProtoReceived(std::move(message));
  }
}

void BlimpCompositor::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  client_->SendWebGestureEvent(render_widget_id_, gesture_event);
}

void BlimpCompositor::SetVisibleInternal(bool visible) {
  if (!host_)
    return;

  VLOG(1) << "Setting visibility to: " << visible
          << " for render widget: " << render_widget_id_;

  if (visible && window_ != gfx::kNullAcceleratedWidget) {
    // If we're supposed to be visible and we have a valid
    // gfx::AcceleratedWidget make our compositor visible. If the compositor
    // had an outstanding output surface request, trigger the request again so
    // we build the output surface.
    host_->SetVisible(true);
    if (output_surface_request_pending_)
      HandlePendingOutputSurfaceRequest();
  } else if (!visible) {
    // If not visible, hide the compositor and have it drop it's output
    // surface.
    host_->SetVisible(false);
    if (!host_->output_surface_lost()) {
      host_->ReleaseOutputSurface();
    }
  }
}

void BlimpCompositor::CreateLayerTreeHost(
    const cc::proto::InitializeImpl& initialize_message) {
  DCHECK(!host_);
  VLOG(1) << "Creating LayerTreeHost for render widget: " << render_widget_id_;

  // Create the LayerTreeHost
  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = client_->GetTaskGraphRunner();
  params.gpu_memory_buffer_manager = client_->GetGpuMemoryBufferManager();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.image_serialization_processor =
      client_->GetImageSerializationProcessor();
  params.settings = client_->GetLayerTreeSettings();
  params.animation_host = cc::AnimationHost::CreateMainInstance();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      client_->GetCompositorTaskRunner();

  host_ =
      cc::LayerTreeHost::CreateRemoteClient(this /* remote_proto_channel */,
                                            compositor_task_runner, &params);

  // Now that we have a host, set the visiblity on it correctly.
  SetVisibleInternal(host_should_be_visible_);

  DCHECK(!input_manager_);
  input_manager_ =
      BlimpInputManager::Create(this,
                                base::ThreadTaskRunnerHandle::Get(),
                                compositor_task_runner,
                                host_->GetInputHandler());
}

void BlimpCompositor::DestroyLayerTreeHost() {
  DCHECK(host_);
  VLOG(1) << "Destroying LayerTreeHost for render widget: "
          << render_widget_id_;
  // Tear down the output surface connection with the old LayerTreeHost
  // instance.
  SetVisibleInternal(false);

  // Destroy the old LayerTreeHost state.
  host_.reset();

  // Destroy the old input manager state.
  // It is important to destroy the LayerTreeHost before destroying the input
  // manager as it has a reference to the cc::InputHandlerClient owned by the
  // BlimpInputManager.
  input_manager_.reset();

  // Reset other state.
  output_surface_request_pending_ = false;

  // Make sure we don't have a receiver at this point.
  DCHECK(!remote_proto_channel_receiver_);
}

void BlimpCompositor::HandlePendingOutputSurfaceRequest() {
  DCHECK(output_surface_request_pending_);

  // We might have had a request from a LayerTreeHost that was then
  // hidden (and hidden means we don't have a native surface).
  // Also make sure we only handle this once.
  if (!host_->visible() || window_ == gfx::kNullAcceleratedWidget)
    return;

  scoped_refptr<BlimpContextProvider> context_provider =
      BlimpContextProvider::Create(window_,
                                   client_->GetGpuMemoryBufferManager());

  host_->SetOutputSurface(
      base::WrapUnique(new BlimpOutputSurface(context_provider)));
  output_surface_request_pending_ = false;
}

}  // namespace client
}  // namespace blimp
