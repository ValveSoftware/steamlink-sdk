// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/compositor_mus_connection.h"

#include "base/single_thread_task_runner.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/latency_info.h"
#include "ui/events/mojo/event.mojom.h"

using ui::mojom::EventResult;

namespace {

void DoNothingWithEventResult(EventResult result) {}

gfx::Point GetScreenLocationFromEvent(const ui::LocatedEvent& event) {
  return event.root_location();
}

}  // namespace

namespace content {

CompositorMusConnection::CompositorMusConnection(
    int routing_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request,
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

void CompositorMusConnection::AttachCompositorFrameSinkOnMainThread(
    std::unique_ptr<ui::WindowCompositorFrameSinkBinding>
        compositor_frame_sink_binding) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &CompositorMusConnection::AttachCompositorFrameSinkOnCompositorThread,
          this, base::Passed(std::move(compositor_frame_sink_binding))));
}

CompositorMusConnection::~CompositorMusConnection() {
  base::AutoLock auto_lock(window_tree_client_lock_);
  // Destruction must happen on the compositor task runner.
  DCHECK(!window_tree_client_);
}

void CompositorMusConnection::AttachCompositorFrameSinkOnCompositorThread(
    std::unique_ptr<ui::WindowCompositorFrameSinkBinding>
        compositor_frame_sink_binding) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  window_compositor_frame_sink_binding_ =
      std::move(compositor_frame_sink_binding);
  if (root_) {
    root_->AttachCompositorFrameSink(
        ui::mojom::CompositorFrameSinkType::DEFAULT,
        std::move(window_compositor_frame_sink_binding_));
  }
}

void CompositorMusConnection::CreateWindowTreeClientOnCompositorThread(
    mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  DCHECK(!window_tree_client_);
  std::unique_ptr<ui::WindowTreeClient> window_tree_client =
      base::MakeUnique<ui::WindowTreeClient>(this, nullptr, std::move(request));
  base::AutoLock auto_lock(window_tree_client_lock_);
  window_tree_client_ = std::move(window_tree_client);
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
    ui::ScopedWebInputEvent web_event,
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

std::unique_ptr<blink::WebInputEvent> CompositorMusConnection::Convert(
    const ui::Event& event) {
  if (event.IsMousePointerEvent()) {
    const ui::MouseEvent mouse_event(*event.AsPointerEvent());
    blink::WebMouseEvent blink_event = ui::MakeWebMouseEvent(
        mouse_event, base::Bind(&GetScreenLocationFromEvent));
    return base::MakeUnique<blink::WebMouseEvent>(blink_event);
  } else if (event.IsTouchPointerEvent()) {
    ui::TouchEvent touch_event(*event.AsPointerEvent());
    pointer_state_.OnTouch(touch_event);
    blink::WebTouchEvent blink_event = ui::CreateWebTouchEventFromMotionEvent(
        pointer_state_, touch_event.may_cause_scrolling());
    pointer_state_.CleanupRemovedTouchPoints(touch_event);
    return base::MakeUnique<blink::WebTouchEvent>(blink_event);
  } else if (event.IsMouseWheelEvent()) {
    blink::WebMouseWheelEvent blink_event = ui::MakeWebMouseWheelEvent(
        *event.AsMouseWheelEvent(), base::Bind(&GetScreenLocationFromEvent));
    return base::MakeUnique<blink::WebMouseWheelEvent>(blink_event);
  } else if (event.IsKeyEvent()) {
    blink::WebKeyboardEvent blink_event =
        ui::MakeWebKeyboardEvent(*event.AsKeyEvent());
    return base::MakeUnique<blink::WebKeyboardEvent>(blink_event);
  }
  return nullptr;
}

void CompositorMusConnection::DeleteWindowTreeClient() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<ui::WindowTreeClient> window_tree_client;
  {
    base::AutoLock auto_lock(window_tree_client_lock_);
    window_tree_client = std::move(window_tree_client_);
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CompositorMusConnection::OnConnectionLostOnMainThread, this));
}

void CompositorMusConnection::OnEmbed(ui::Window* root) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  root_ = root;
  root_->set_input_event_handler(this);
  if (window_compositor_frame_sink_binding_) {
    root->AttachCompositorFrameSink(
        ui::mojom::CompositorFrameSinkType::DEFAULT,
        std::move(window_compositor_frame_sink_binding_));
  }
}

void CompositorMusConnection::OnEmbedRootDestroyed(ui::Window* window) {
  DeleteWindowTreeClient();
}

void CompositorMusConnection::OnLostConnection(ui::WindowTreeClient* client) {
  DeleteWindowTreeClient();
}

void CompositorMusConnection::OnPointerEventObserved(
    const ui::PointerEvent& event,
    ui::Window* target) {
  // Compositor does not use StartPointerWatcher().
}

void CompositorMusConnection::OnWindowInputEvent(
    ui::Window* window,
    const ui::Event& event,
    std::unique_ptr<base::Callback<void(EventResult)>>* ack_callback) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());

  // Take ownership of the callback, indicating that we will handle it.
  std::unique_ptr<base::Callback<void(EventResult)>> callback =
      std::move(*ack_callback);
  ui::ScopedWebInputEvent web_event(Convert(event).release());
  // TODO(sad): We probably need to plumb LatencyInfo through Mus.
  ui::LatencyInfo info;
  input_handler_manager_->HandleInputEvent(
      routing_id_, std::move(web_event), info,
      base::Bind(
          &CompositorMusConnection::DidHandleWindowInputEventAndOverscroll,
          this, base::Passed(std::move(callback))));
}

void CompositorMusConnection::DidHandleWindowInputEventAndOverscroll(
    std::unique_ptr<base::Callback<void(EventResult)>> ack_callback,
    InputEventAckState ack_state,
    ui::ScopedWebInputEvent web_event,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params) {
  // TODO(jonross): We probably need to ack the event based on the consumed
  // state.
  if (ack_state != INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    // We took the ownership of the callback, so we need to send the ack, and
    // mark the event as not consumed to preserve existing behavior.
    ack_callback->Run(EventResult::UNHANDLED);
    return;
  }
  base::Callback<void(EventResult)> ack =
      base::Bind(&::DoNothingWithEventResult);
  const bool send_ack =
      ui::WebInputEventTraits::ShouldBlockEventStream(*web_event);
  if (send_ack) {
    // Ultimately, this ACK needs to go back to the Mus client lib which is not
    // thread-safe and lives on the compositor thread. For ACKs that are passed
    // to the main thread we pass them back to the compositor thread via
    // OnWindowInputEventAckOnMainThread.
    ack =
        base::Bind(&CompositorMusConnection::OnWindowInputEventAckOnMainThread,
                   this, *ack_callback);
  } else {
    // We took the ownership of the callback, so we need to send the ack, and
    // mark the event as not consumed to preserve existing behavior.
    ack_callback->Run(EventResult::UNHANDLED);
  }
  ack_callback.reset();

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CompositorMusConnection::OnWindowInputEventOnMainThread, this,
                 base::Passed(std::move(web_event)), ack));
}

}  // namespace content
