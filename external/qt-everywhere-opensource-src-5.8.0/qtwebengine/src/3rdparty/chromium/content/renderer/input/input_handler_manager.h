// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/renderer/render_view_impl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class InputHandler;
struct InputHandlerScrollResult;
}

namespace blink {
class WebInputEvent;
class WebMouseWheelEvent;
}

namespace scheduler {
class RendererScheduler;
}

namespace content {

class InputHandlerWrapper;
class SynchronousInputHandlerProxyClient;
class InputHandlerManagerClient;
struct DidOverscrollParams;

// InputHandlerManager class manages InputHandlerProxy instances for
// the WebViews in this renderer.
class CONTENT_EXPORT InputHandlerManager {
 public:
  // |task_runner| is the SingleThreadTaskRunner of the compositor thread. The
  // underlying MessageLoop and supplied |client| and the |renderer_scheduler|
  // must outlive this object. The RendererScheduler needs to know when input
  // events and fling animations occur, which is why it's passed in here.
  InputHandlerManager(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      InputHandlerManagerClient* client,
      SynchronousInputHandlerProxyClient* sync_handler_client,
      scheduler::RendererScheduler* renderer_scheduler);
  virtual ~InputHandlerManager();

  // Callable from the main thread only.
  void AddInputHandler(int routing_id,
                       const base::WeakPtr<cc::InputHandler>& input_handler,
                       const base::WeakPtr<RenderViewImpl>& render_view_impl,
                       bool enable_smooth_scrolling);

  void RegisterRoutingID(int routing_id);
  void UnregisterRoutingID(int routing_id);

  void ObserveGestureEventAndResultOnMainThread(
      int routing_id,
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void NotifyInputEventHandledOnMainThread(int routing_id,
                                           blink::WebInputEvent::Type,
                                           InputEventAckState);

  // Callback only from the compositor's thread.
  void RemoveInputHandler(int routing_id);

  // Called from the compositor's thread.
  virtual InputEventAckState HandleInputEvent(
      int routing_id,
      const blink::WebInputEvent* input_event,
      ui::LatencyInfo* latency_info);

  // Called from the compositor's thread.
  void DidOverscroll(int routing_id, const DidOverscrollParams& params);

  // Called from the compositor's thread.
  void DidStartFlinging(int routing_id);
  void DidStopFlinging(int routing_id);

  // Called from the compositor's thread.
  void DidAnimateForInput();

 private:
  // Called from the compositor's thread.
  void AddInputHandlerOnCompositorThread(
      int routing_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderViewImpl>& render_view_impl,
      bool enable_smooth_scrolling);

  void RegisterRoutingIDOnCompositorThread(int routing_id);
  void UnregisterRoutingIDOnCompositorThread(int routing_id);

  void ObserveWheelEventAndResultOnCompositorThread(
      int routing_id,
      const blink::WebMouseWheelEvent& wheel_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void ObserveGestureEventAndResultOnCompositorThread(
      int routing_id,
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void NotifyInputEventHandledOnCompositorThread(int routing_id,
                                                 blink::WebInputEvent::Type,
                                                 InputEventAckState);

  typedef base::ScopedPtrHashMap<int,  // routing_id
                                 std::unique_ptr<InputHandlerWrapper>>
      InputHandlerMap;
  InputHandlerMap input_handlers_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InputHandlerManagerClient* const client_;
  // May be null.
  SynchronousInputHandlerProxyClient* const synchronous_handler_proxy_client_;
  scheduler::RendererScheduler* const renderer_scheduler_;  // Not owned.
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_
