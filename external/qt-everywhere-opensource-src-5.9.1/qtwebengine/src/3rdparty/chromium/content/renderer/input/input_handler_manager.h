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
#include "ui/events/blink/input_handler_proxy.h"

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

namespace blink {
namespace scheduler {
class RendererScheduler;
}
}

namespace ui {
struct DidOverscrollParams;
}

namespace content {

class InputHandlerWrapper;
class SynchronousInputHandlerProxyClient;
class InputHandlerManagerClient;

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
      blink::scheduler::RendererScheduler* renderer_scheduler);
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
  void ProcessRafAlignedInputOnMainThread(int routing_id);

  // Callback only from the compositor's thread.
  void RemoveInputHandler(int routing_id);

  using InputEventAckStateCallback =
      base::Callback<void(InputEventAckState,
                          ui::ScopedWebInputEvent,
                          const ui::LatencyInfo&,
                          std::unique_ptr<ui::DidOverscrollParams>)>;
  // Called from the compositor's thread.
  virtual void HandleInputEvent(int routing_id,
                                ui::ScopedWebInputEvent input_event,
                                const ui::LatencyInfo& latency_info,
                                const InputEventAckStateCallback& callback);

  // Called from the compositor's thread.
  void DidOverscroll(int routing_id, const ui::DidOverscrollParams& params);

  // Called from the compositor's thread.
  void DidStopFlinging(int routing_id);

  // Called from the compositor's thread.
  void DidAnimateForInput();

  // Called from the compositor's thread.
  void NeedsMainFrame(int routing_id);

  // Called from the compositor's thread.
  void DispatchNonBlockingEventToMainThread(
      int routing_id,
      ui::ScopedWebInputEvent event,
      const ui::LatencyInfo& latency_info);

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

  void DidHandleInputEventAndOverscroll(
      const InputEventAckStateCallback& callback,
      ui::InputHandlerProxy::EventDisposition event_disposition,
      ui::ScopedWebInputEvent input_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params);

  typedef base::ScopedPtrHashMap<int,  // routing_id
                                 std::unique_ptr<InputHandlerWrapper>>
      InputHandlerMap;
  InputHandlerMap input_handlers_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InputHandlerManagerClient* const client_;
  // May be null.
  SynchronousInputHandlerProxyClient* const synchronous_handler_proxy_client_;
  blink::scheduler::RendererScheduler* const renderer_scheduler_;  // Not owned.

  base::WeakPtrFactory<InputHandlerManager> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_
