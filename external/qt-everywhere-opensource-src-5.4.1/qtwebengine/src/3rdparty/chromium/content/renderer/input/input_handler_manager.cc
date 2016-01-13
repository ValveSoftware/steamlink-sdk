// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_manager.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/input/input_handler.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "content/renderer/input/input_handler_wrapper.h"

using blink::WebInputEvent;

namespace content {

namespace {

InputEventAckState InputEventDispositionToAck(
    InputHandlerProxy::EventDisposition disposition) {
  switch (disposition) {
    case InputHandlerProxy::DID_HANDLE:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case InputHandlerProxy::DID_NOT_HANDLE:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case InputHandlerProxy::DROP_EVENT:
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }
  NOTREACHED();
  return INPUT_EVENT_ACK_STATE_UNKNOWN;
}

} // namespace

InputHandlerManager::InputHandlerManager(
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    InputHandlerManagerClient* client)
    : message_loop_proxy_(message_loop_proxy),
      client_(client) {
  DCHECK(client_);
  client_->SetBoundHandler(base::Bind(&InputHandlerManager::HandleInputEvent,
                                      base::Unretained(this)));
}

InputHandlerManager::~InputHandlerManager() {
  client_->SetBoundHandler(InputHandlerManagerClient::Handler());
}

void InputHandlerManager::AddInputHandler(
    int routing_id,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  if (message_loop_proxy_->BelongsToCurrentThread()) {
    AddInputHandlerOnCompositorThread(routing_id,
                                      base::MessageLoopProxy::current(),
                                      input_handler,
                                      render_view_impl);
  } else {
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&InputHandlerManager::AddInputHandlerOnCompositorThread,
                   base::Unretained(this),
                   routing_id,
                   base::MessageLoopProxy::current(),
                   input_handler,
                   render_view_impl));
  }
}

void InputHandlerManager::AddInputHandlerOnCompositorThread(
    int routing_id,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  // The handler could be gone by this point if the compositor has shut down.
  if (!input_handler)
    return;

  // The same handler may be registered for a route multiple times.
  if (input_handlers_.count(routing_id) != 0)
    return;

  TRACE_EVENT1("input",
      "InputHandlerManager::AddInputHandlerOnCompositorThread",
      "result", "AddingRoute");
  client_->DidAddInputHandler(routing_id, input_handler.get());
  input_handlers_.add(routing_id,
      make_scoped_ptr(new InputHandlerWrapper(this,
          routing_id, main_loop, input_handler, render_view_impl)));
}

void InputHandlerManager::RemoveInputHandler(int routing_id) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(input_handlers_.contains(routing_id));

  TRACE_EVENT0("input", "InputHandlerManager::RemoveInputHandler");

  client_->DidRemoveInputHandler(routing_id);
  input_handlers_.erase(routing_id);
}

InputEventAckState InputHandlerManager::HandleInputEvent(
    int routing_id,
    const WebInputEvent* input_event,
    ui::LatencyInfo* latency_info) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  InputHandlerMap::iterator it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end()) {
    TRACE_EVENT1("input", "InputHandlerManager::HandleInputEvent",
                 "result", "NoInputHandlerFound");
    // Oops, we no longer have an interested input handler..
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }

  InputHandlerProxy* proxy = it->second->input_handler_proxy();
  return InputEventDispositionToAck(
      proxy->HandleInputEventWithLatencyInfo(*input_event, latency_info));
}

void InputHandlerManager::DidOverscroll(int routing_id,
                                        const DidOverscrollParams& params) {
  client_->DidOverscroll(routing_id, params);
}

void InputHandlerManager::DidStopFlinging(int routing_id) {
  client_->DidStopFlinging(routing_id);
}

}  // namespace content
