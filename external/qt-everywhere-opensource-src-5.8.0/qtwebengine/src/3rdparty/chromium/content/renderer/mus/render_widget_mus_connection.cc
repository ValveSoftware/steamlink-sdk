// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/render_widget_mus_connection.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/mus/public/cpp/context_provider.h"
#include "components/mus/public/cpp/output_surface.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "components/mus/public/interfaces/surface.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/renderer/mus/compositor_mus_connection.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "services/shell/public/cpp/connector.h"

namespace content {

namespace {

typedef std::map<int, RenderWidgetMusConnection*> ConnectionMap;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;
}

void RenderWidgetMusConnection::Bind(
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  compositor_mus_connection_ = new CompositorMusConnection(
      routing_id_, render_thread->GetCompositorMainThreadTaskRunner(),
      render_thread->compositor_task_runner(), std::move(request),
      render_thread->input_handler_manager());
  if (window_surface_binding_) {
    compositor_mus_connection_->AttachSurfaceOnMainThread(
        std::move(window_surface_binding_));
  }
}

std::unique_ptr<cc::OutputSurface>
RenderWidgetMusConnection::CreateOutputSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!window_surface_binding_);
  scoped_refptr<cc::ContextProvider> context_provider(new mus::ContextProvider(
      MojoShellConnection::GetForProcess()->GetConnector()));

  std::unique_ptr<cc::OutputSurface> surface(new mus::OutputSurface(
      context_provider, mus::WindowSurface::Create(&window_surface_binding_)));
  if (compositor_mus_connection_) {
    compositor_mus_connection_->AttachSurfaceOnMainThread(
        std::move(window_surface_binding_));
  }
  return surface;
}

// static
RenderWidgetMusConnection* RenderWidgetMusConnection::Get(int routing_id) {
  auto it = g_connections.Get().find(routing_id);
  if (it != g_connections.Get().end())
    return it->second;
  return nullptr;
}

// static
RenderWidgetMusConnection* RenderWidgetMusConnection::GetOrCreate(
    int routing_id) {
  RenderWidgetMusConnection* connection = Get(routing_id);
  if (!connection) {
    connection = new RenderWidgetMusConnection(routing_id);
    g_connections.Get().insert(std::make_pair(routing_id, connection));
  }
  return connection;
}

RenderWidgetMusConnection::RenderWidgetMusConnection(int routing_id)
    : routing_id_(routing_id), input_handler_(nullptr) {
  DCHECK(routing_id);
}

RenderWidgetMusConnection::~RenderWidgetMusConnection() {}

void RenderWidgetMusConnection::FocusChangeComplete() {
  NOTIMPLEMENTED();
}

bool RenderWidgetMusConnection::HasTouchEventHandlersAt(
    const gfx::Point& point) const {
  return true;
}

void RenderWidgetMusConnection::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const gfx::Vector2dF& wheel_unused_delta,
    bool event_processed) {
  NOTIMPLEMENTED();
}

void RenderWidgetMusConnection::OnDidHandleKeyEvent() {
  NOTIMPLEMENTED();
}

void RenderWidgetMusConnection::OnDidOverscroll(
    const DidOverscrollParams& params) {
  NOTIMPLEMENTED();
}

void RenderWidgetMusConnection::OnInputEventAck(
    std::unique_ptr<InputEventAck> input_event_ack) {
  DCHECK(!pending_ack_.is_null());
  pending_ack_.Run(input_event_ack->state ==
                           InputEventAckState::INPUT_EVENT_ACK_STATE_CONSUMED
                       ? mus::mojom::EventResult::HANDLED
                       : mus::mojom::EventResult::UNHANDLED);
  pending_ack_.Reset();
}

void RenderWidgetMusConnection::NotifyInputEventHandled(
    blink::WebInputEvent::Type handled_type,
    InputEventAckState ack_result) {
  NOTIMPLEMENTED();
}

void RenderWidgetMusConnection::SetInputHandler(
    RenderWidgetInputHandler* input_handler) {
  DCHECK(!input_handler_);
  input_handler_ = input_handler;
}

void RenderWidgetMusConnection::UpdateTextInputState(
    ShowIme show_ime,
    ChangeSource change_source) {
  NOTIMPLEMENTED();
}

bool RenderWidgetMusConnection::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetMusConnection::WillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  // TODO(fsamuel): NOTIMPLEMENTED() is too noisy.
  // NOTIMPLEMENTED();
  return false;
}

void RenderWidgetMusConnection::OnConnectionLost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_connections.Get().erase(routing_id_);
  delete this;
}

void RenderWidgetMusConnection::OnWindowInputEvent(
    std::unique_ptr<blink::WebInputEvent> input_event,
    const base::Callback<void(mus::mojom::EventResult)>& ack) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If we don't yet have a RenderWidgetInputHandler then we don't yet have
  // an initialized RenderWidget.
  if (!input_handler_) {
    ack.Run(mus::mojom::EventResult::UNHANDLED);
    return;
  }
  // TODO(fsamuel): It would be nice to add this DCHECK but the reality is an
  // event could timeout and we could receive the next event before we ack the
  // previous event.
  // DCHECK(pending_ack_.is_null());
  pending_ack_ = ack;
  // TODO(fsamuel, sadrul): Track real latency info.
  ui::LatencyInfo latency_info;
  input_handler_->HandleInputEvent(*input_event, latency_info,
                                   DISPATCH_TYPE_BLOCKING);
}

}  // namespace content
