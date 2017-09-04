// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_filter.h"

#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/vector2d_f.h"

using blink::WebInputEvent;
using ui::DidOverscrollParams;

#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(name, ...) \
  case name::ID:                    \
    return #name;

const char* GetInputMessageTypeName(const IPC::Message& message) {
  switch (message.type()) {
#include "content/common/input_messages.h"
    default:
      NOTREACHED() << "Invalid message type: " << message.type();
      break;
  };
  return "NonInputMsgType";
}

namespace content {

InputEventFilter::InputEventFilter(
    const base::Callback<void(const IPC::Message&)>& main_listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner)
    : main_task_runner_(main_task_runner),
      main_listener_(main_listener),
      sender_(NULL),
      target_task_runner_(target_task_runner),
      input_handler_manager_(NULL),
      renderer_scheduler_(NULL) {
  DCHECK(target_task_runner_.get());
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  renderer_scheduler_ =
      render_thread_impl ? render_thread_impl->GetRendererScheduler() : nullptr;
}

void InputEventFilter::SetInputHandlerManager(
    InputHandlerManager* input_handler_manager) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  input_handler_manager_ = input_handler_manager;
}

void InputEventFilter::RegisterRoutingID(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.insert(routing_id);
  route_queues_[routing_id] = new MainThreadEventQueue(
      routing_id, this, main_task_runner_, renderer_scheduler_);
}

void InputEventFilter::UnregisterRoutingID(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.erase(routing_id);
  route_queues_.erase(routing_id);
}

void InputEventFilter::DidOverscroll(int routing_id,
                                     const DidOverscrollParams& params) {
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_DidOverscroll(routing_id, params)));
}

void InputEventFilter::DidStopFlinging(int routing_id) {
  SendMessage(base::MakeUnique<InputHostMsg_DidStopFlinging>(routing_id));
}

void InputEventFilter::DispatchNonBlockingEventToMainThread(
    int routing_id,
    ui::ScopedWebInputEvent event,
    const ui::LatencyInfo& latency_info) {
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  RouteQueueMap::iterator iter = route_queues_.find(routing_id);
  if (iter != route_queues_.end()) {
    iter->second->HandleEvent(std::move(event), latency_info,
                              DISPATCH_TYPE_NON_BLOCKING,
                              INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  }
}

void InputEventFilter::NotifyInputEventHandled(int routing_id,
                                               blink::WebInputEvent::Type type,
                                               InputEventAckState ack_result) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<MainThreadEventQueue> queue;
  {
    base::AutoLock locked(routes_lock_);
    RouteQueueMap::iterator iter = route_queues_.find(routing_id);
    if (iter == route_queues_.end() || !iter->second)
      return;
    queue = iter->second;
  }

  queue->EventHandled(type, ack_result);
}

void InputEventFilter::ProcessRafAlignedInput(int routing_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<MainThreadEventQueue> queue;
  {
    base::AutoLock locked(routes_lock_);
    RouteQueueMap::iterator iter = route_queues_.find(routing_id);
    if (iter == route_queues_.end() || !iter->second)
      return;
    queue = iter->second;
  }

  queue->DispatchRafAlignedInput();
}

void InputEventFilter::OnFilterAdded(IPC::Channel* channel) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = channel;
}

void InputEventFilter::OnFilterRemoved() {
  sender_ = NULL;
}

void InputEventFilter::OnChannelClosing() {
  sender_ = NULL;
}

// This function returns true if the IPC message is one that the compositor
// thread can handle *or* one that needs to preserve relative order with
// messages that the compositor thread can handle. Returning true for a message
// type means that the message will go through an extra copy and thread hop, so
// use with care.
static bool RequiresThreadBounce(const IPC::Message& message) {
  return IPC_MESSAGE_ID_CLASS(message.type()) == InputMsgStart;
}

bool InputEventFilter::OnMessageReceived(const IPC::Message& message) {
  if (!RequiresThreadBounce(message))
    return false;

  // If TimeTicks is not consistent across processes we cannot use the event's
  // platform timestamp in this process. Instead the time that the event is
  // received on the IO thread is used as the event's timestamp.
  base::TimeTicks received_time;
  if (!base::TimeTicks::IsConsistentAcrossProcesses())
    received_time = base::TimeTicks::Now();

  TRACE_EVENT0("input", "InputEventFilter::OnMessageReceived::InputMessage");

  {
    base::AutoLock locked(routes_lock_);
    if (routes_.find(message.routing_id()) == routes_.end())
      return false;
  }

  bool postedTask = target_task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputEventFilter::ForwardToHandler, this, message,
                            received_time));
  LOG_IF(WARNING, !postedTask) << "PostTask failed";
  return true;
}

InputEventFilter::~InputEventFilter() {}

void InputEventFilter::ForwardToHandler(const IPC::Message& message,
                                        base::TimeTicks received_time) {
  DCHECK(input_handler_manager_);
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("input", "InputEventFilter::ForwardToHandler",
               "message_type", GetInputMessageTypeName(message));

  if (message.type() != InputMsg_HandleInputEvent::ID) {
    TRACE_EVENT_INSTANT0(
        "input",
        "InputEventFilter::ForwardToHandler::ForwardToMainListener",
        TRACE_EVENT_SCOPE_THREAD);
    CHECK(main_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(main_listener_, message)))
        << "PostTask failed";
    return;
  }

  int routing_id = message.routing_id();
  InputMsg_HandleInputEvent::Param params;
  if (!InputMsg_HandleInputEvent::Read(&message, &params))
    return;
  ui::ScopedWebInputEvent event =
      ui::WebInputEventTraits::Clone(*std::get<0>(params));
  ui::LatencyInfo latency_info = std::get<1>(params);
  InputEventDispatchType dispatch_type = std::get<2>(params);

  DCHECK(event);
  DCHECK(dispatch_type == DISPATCH_TYPE_BLOCKING ||
         dispatch_type == DISPATCH_TYPE_NON_BLOCKING);

  if (!received_time.is_null())
    event->timeStampSeconds = ui::EventTimeStampToSeconds(received_time);

  input_handler_manager_->HandleInputEvent(
      routing_id, std::move(event), latency_info,
      base::Bind(&InputEventFilter::DidForwardToHandlerAndOverscroll, this,
                 routing_id, dispatch_type));
};

void InputEventFilter::DidForwardToHandlerAndOverscroll(
    int routing_id,
    InputEventDispatchType dispatch_type,
    InputEventAckState ack_state,
    ui::ScopedWebInputEvent event,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<DidOverscrollParams> overscroll_params) {
  bool send_ack = dispatch_type == DISPATCH_TYPE_BLOCKING;
  uint32_t unique_touch_event_id =
      ui::WebInputEventTraits::GetUniqueTouchEventId(*event);
  WebInputEvent::Type type = event->type;

  if (ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
      ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
      ack_state == INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    DCHECK(!overscroll_params);
    RouteQueueMap::iterator iter = route_queues_.find(routing_id);
    if (iter != route_queues_.end()) {
      send_ack &= iter->second->HandleEvent(std::move(event), latency_info,
                                            dispatch_type, ack_state);
    }
  }
  event.reset();

  if (!send_ack)
    return;

  InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, type, ack_state,
                    latency_info, std::move(overscroll_params),
                    unique_touch_event_id);
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_HandleInputEvent_ACK(routing_id, ack)));
}

void InputEventFilter::SendMessage(std::unique_ptr<IPC::Message> message) {
  CHECK(io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputEventFilter::SendMessageOnIOThread, this,
                            base::Passed(&message))))
      << "PostTask failed";
}

void InputEventFilter::SendMessageOnIOThread(
    std::unique_ptr<IPC::Message> message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!sender_)
    return;  // Filter was removed.

  bool success = sender_->Send(message.release());
  if (success)
    return;
  static size_t s_send_failure_count_ = 0;
  s_send_failure_count_++;
  base::debug::SetCrashKeyValue("input-event-filter-send-failure",
                                base::IntToString(s_send_failure_count_));
}

void InputEventFilter::HandleEventOnMainThread(
    int routing_id,
    const blink::WebInputEvent* event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  TRACE_EVENT_INSTANT0("input", "InputEventFilter::HandlEventOnMainThread",
                       TRACE_EVENT_SCOPE_THREAD);
  IPC::Message new_msg =
      InputMsg_HandleInputEvent(routing_id, event, latency_info, dispatch_type);
  main_listener_.Run(new_msg);
}

void InputEventFilter::SendInputEventAck(int routing_id,
                                         blink::WebInputEvent::Type type,
                                         InputEventAckState ack_result,
                                         uint32_t touch_event_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  InputEventAck ack(InputEventAckSource::MAIN_THREAD, type, ack_result,
                    touch_event_id);
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_HandleInputEvent_ACK(routing_id, ack)));
}

void InputEventFilter::NeedsMainFrame(int routing_id) {
  if (target_task_runner_->BelongsToCurrentThread()) {
    input_handler_manager_->NeedsMainFrame(routing_id);
    return;
  }

  CHECK(target_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InputEventFilter::NeedsMainFrame, this, routing_id)))
      << "PostTask failed";
}

}  // namespace content
