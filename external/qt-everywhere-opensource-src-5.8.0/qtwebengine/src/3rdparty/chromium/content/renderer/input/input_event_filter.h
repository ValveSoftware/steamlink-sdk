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
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ui {
class SynchronousInputHandlerProxy;
}

namespace IPC {
class Listener;
class Sender;
}

// This class can be used to intercept InputMsg_HandleInputEvent messages
// and have them be delivered to a target thread.  Input events are filtered
// based on routing_id (see AddRoute and RemoveRoute).
//
// The user of this class provides an instance of InputEventFilter::Handler,
// which will be passed WebInputEvents on the target thread.
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
  void SetBoundHandler(const Handler& handler) override;
  void RegisterRoutingID(int routing_id) override;
  void UnregisterRoutingID(int routing_id) override;
  void DidOverscroll(int routing_id,
                     const DidOverscrollParams& params) override;
  void DidStartFlinging(int routing_id) override;
  void DidStopFlinging(int routing_id) override;
  void NotifyInputEventHandled(int routing_id,
                               blink::WebInputEvent::Type type,
                               InputEventAckState ack_result) override;

  // IPC::MessageFilter methods:
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // MainThreadEventQueueClient methods:
  void SendEventToMainThread(int routing_id,
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

 private:
  ~InputEventFilter() override;

  void ForwardToHandler(const IPC::Message& message);
  void SendMessage(std::unique_ptr<IPC::Message> message);
  void SendMessageOnIOThread(std::unique_ptr<IPC::Message> message);
  void SetIsFlingingInMainThreadEventQueue(int routing_id, bool is_flinging);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::Callback<void(const IPC::Message&)> main_listener_;

  // The sender_ only gets invoked on the thread corresponding to io_loop_.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  IPC::Sender* sender_;

  // The handler_ only gets Run on the thread corresponding to target_loop_.
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;
  Handler handler_;

  // Protects access to routes_.
  base::Lock routes_lock_;

  // Indicates the routing_ids for which input events should be filtered.
  std::set<int> routes_;

  using RouteQueueMap =
      std::unordered_map<int, std::unique_ptr<MainThreadEventQueue>>;
  RouteQueueMap route_queues_;

  // Used to intercept overscroll notifications while an event is being
  // dispatched.  If the event causes overscroll, the overscroll metadata can be
  // bundled in the event ack, saving an IPC.  Note that we must continue
  // supporting overscroll IPC notifications due to fling animation updates.
  std::unique_ptr<DidOverscrollParams>* current_overscroll_params_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_
