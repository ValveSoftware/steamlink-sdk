// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/compositor_mus_connection.h"

#include "base/single_thread_task_runner.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "mojo/converters/blink/blink_input_events_type_converters.h"
#include "ui/events/latency_info.h"
#include "ui/events/mojo/event.mojom.h"

using mus::mojom::EventResult;

namespace {

void DoNothingWithEventResult(EventResult result) {}

}  // namespace

namespace content {

CompositorMusConnection::CompositorMusConnection(
    int routing_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request,
    InputHandlerManager* input_handler_manager)
    : routing_id_(routing_id),
      root_(nullptr),
      main_task_runner_(main_task_runner),
      compositor_task_runner_(compositor_task_runner),
      input_handler_manager_(input_handler_manager) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CompositorMusConnection::
                                CreateWindowTreeClientOnCompositorThread,
                            this, base::Passed(std::move(request))));
}

void CompositorMusConnection::AttachSurfaceOnMainThread(
    std::unique_ptr<mus::WindowSurfaceBinding> surface_binding) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CompositorMusConnection::AttachSurfaceOnCompositorThread,
                 this, base::Passed(std::move(surface_binding))));
}

CompositorMusConnection::~CompositorMusConnection() {}

void CompositorMusConnection::AttachSurfaceOnCompositorThread(
    std::unique_ptr<mus::WindowSurfaceBinding> surface_binding) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  window_surface_binding_ = std::move(surface_binding);
  if (root_) {
    root_->AttachSurface(mus::mojom::SurfaceType::DEFAULT,
                         std::move(window_surface_binding_));
  }
}

void CompositorMusConnection::CreateWindowTreeClientOnCompositorThread(
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  new mus::WindowTreeClient(this, nullptr, std::move(request));
}

void CompositorMusConnection::OnConnectionLostOnMainThread() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  RenderWidgetMusConnection* connection =
      RenderWidgetMusConnection::Get(routing_id_);
  if (!connection)
    return;
  connection->OnConnectionLost();
}

void CompositorMusConnection::OnWindowInputEventOnMainThread(
    std::unique_ptr<blink::WebInputEvent> web_event,
    const base::Callback<void(EventResult)>& ack) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  RenderWidgetMusConnection* connection =
      RenderWidgetMusConnection::Get(routing_id_);
  if (!connection) {
    ack.Run(EventResult::UNHANDLED);
    return;
  }
  connection->OnWindowInputEvent(std::move(web_event), ack);
}

void CompositorMusConnection::OnWindowInputEventAckOnMainThread(
    const base::Callback<void(EventResult)>& ack,
    EventResult result) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  compositor_task_runner_->PostTask(FROM_HERE, base::Bind(ack, result));
}

void CompositorMusConnection::OnWindowTreeClientDestroyed(
    mus::WindowTreeClient* client) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CompositorMusConnection::OnConnectionLostOnMainThread, this));
}

void CompositorMusConnection::OnEmbed(mus::Window* root) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  root_ = root;
  root_->set_input_event_handler(this);
  if (window_surface_binding_) {
    root->AttachSurface(mus::mojom::SurfaceType::DEFAULT,
                        std::move(window_surface_binding_));
  }
}

void CompositorMusConnection::OnEventObserved(const ui::Event& event,
                                              mus::Window* target) {
  // Compositor does not use SetEventObserver().
}

void CompositorMusConnection::OnWindowInputEvent(
    mus::Window* window,
    const ui::Event& event,
    std::unique_ptr<base::Callback<void(EventResult)>>* ack_callback) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<blink::WebInputEvent> web_event(
      mojo::TypeConverter<std::unique_ptr<blink::WebInputEvent>,
                          ui::Event>::Convert(event));
  // TODO(sad): We probably need to plumb LatencyInfo through Mus.
  ui::LatencyInfo info;
  InputEventAckState ack_state = input_handler_manager_->HandleInputEvent(
      routing_id_, web_event.get(), &info);
  // TODO(jonross): We probably need to ack the event based on the consumed
  // state.
  if (ack_state != INPUT_EVENT_ACK_STATE_NOT_CONSUMED)
    return;
  base::Callback<void(EventResult)> ack =
      base::Bind(&::DoNothingWithEventResult);
  const bool send_ack = WebInputEventTraits::ShouldBlockEventStream(*web_event);
  if (send_ack) {
    // Ultimately, this ACK needs to go back to the Mus client lib which is not
    // thread-safe and lives on the compositor thread. For ACKs that are passed
    // to the main thread we pass them back to the compositor thread via
    // OnWindowInputEventAckOnMainThread.
    ack =
        base::Bind(&CompositorMusConnection::OnWindowInputEventAckOnMainThread,
                   this, *ack_callback->get());
    ack_callback->reset();
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CompositorMusConnection::OnWindowInputEventOnMainThread, this,
                 base::Passed(std::move(web_event)), ack));
}

}  // namespace content
