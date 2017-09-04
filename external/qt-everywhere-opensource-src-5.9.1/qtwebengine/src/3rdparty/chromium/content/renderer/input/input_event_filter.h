// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_
#define CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_

#include <queue>
#include <set>
#include <unordered_map>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "ipc/message_filter.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ui {
struct DidOverscrollParams;
}

namespace IPC {
class Sender;
}

// This class can be used to intercept InputMsg_HandleInputEvent messages
// and have them be delivered to a target thread.  Input events are filtered
// based on routing_id (see AddRoute and RemoveRoute).
//
// The user of this class provides an instance of InputHandlerManager via
// |SetInputHandlerManager|. The InputHandlerManager's |HandleInputEvent|
// will be called on the target thread to process the WebInputEvents.
//

namespace content {

class CONTENT_EXPORT InputEventFilter : public InputHandlerManagerClient,
                                        public IPC::MessageFilter,
                                        public MainThreadEventQueueClient {
 public:
  InputEventFilter(
      const base::Callback<void(const IPC::Message&)>& main_listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner);

  // The |handler| is invoked on the thread associated with |target_loop| to
  // handle input events matching the filtered routes.
  //
  // If INPUT_EVENT_ACK_STATE_NOT_CONSUMED is returned by the handler,
  // the original InputMsg_HandleInputEvent message will be delivered to
  // |main_listener| on the main thread.  (The "main thread" in this context is
  // the thread where the InputEventFilter was constructed.)  The responsibility
  // is left to the eventual handler to deliver the corresponding
  // InputHostMsg_HandleInputEvent_ACK.
  //
  void SetInputHandlerManager(InputHandlerManager*) override;
  void RegisterRoutingID(int routing_id) override;
  void UnregisterRoutingID(int routing_id) override;
  void DidOverscroll(int routing_id,
                     const ui::DidOverscrollParams& params) override;
  void DidStopFlinging(int routing_id) override;
  void DispatchNonBlockingEventToMainThread(
      int routing_id,
      ui::ScopedWebInputEvent event,
      const ui::LatencyInfo& latency_info) override;

  void NotifyInputEventHandled(int routing_id,
                               blink::WebInputEvent::Type type,
                               InputEventAckState ack_result) override;
  void ProcessRafAlignedInput(int routing_id) override;

  // IPC::MessageFilter methods:
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // MainThreadEventQueueClient methods:
  void HandleEventOnMainThread(int routing_id,
                               const blink::WebInputEvent* event,
                               const ui::LatencyInfo& latency,
                               InputEventDispatchType dispatch_type) override;
  // Send an InputEventAck IPC message. |touch_event_id| represents
  // the unique event id for the original WebTouchEvent and should
  // be 0 if otherwise. See WebInputEventTraits::GetUniqueTouchEventId.
  void SendInputEventAck(int routing_id,
                         blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) override;

  void NeedsMainFrame(int routing_id) override;

 private:
  ~InputEventFilter() override;

  void ForwardToHandler(const IPC::Message& message,
                        base::TimeTicks received_time);
  void DidForwardToHandlerAndOverscroll(
      int routing_id,
      InputEventDispatchType dispatch_type,
      InputEventAckState ack_state,
      ui::ScopedWebInputEvent event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params);
  void SendMessage(std::unique_ptr<IPC::Message> message);
  void SendMessageOnIOThread(std::unique_ptr<IPC::Message> message);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::Callback<void(const IPC::Message&)> main_listener_;

  // The sender_ only gets invoked on the thread corresponding to io_loop_.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  IPC::Sender* sender_;

  // The |input_handler_manager_| should outlive this class and
  // should only be called back on the |target_task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;
  InputHandlerManager* input_handler_manager_;

  // Protects access to routes_.
  base::Lock routes_lock_;

  // Indicates the routing_ids for which input events should be filtered.
  std::set<int> routes_;

  using RouteQueueMap =
      std::unordered_map<int, scoped_refptr<MainThreadEventQueue>>;
  RouteQueueMap route_queues_;

  blink::scheduler::RendererScheduler* renderer_scheduler_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_
