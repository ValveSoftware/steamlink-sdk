// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_INPUT_BLIMP_INPUT_HANDLER_WRAPPER_H_
#define BLIMP_CLIENT_CORE_INPUT_BLIMP_INPUT_HANDLER_WRAPPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"

namespace cc {
class InputHandler;
}  // namespace cc

namespace blimp {
namespace client {

class BlimpInputManager;

// The BlimpInputHandlerWrapper isolates all input handling processing done on
// the compositor thread from the BlimpInputManager. It takes web gesture events
// from the BlimpInputManager and sends them to the ui::InputHandlerProxy.
// The class is created on the main thread, but becomes bound to the compositor
// thread when it binds to the ui::InputHandlerProxy, and should only be called
// on the compositor thread.
// The creater of this class ensures that the cc::InputHandler is destroyed
// before this class is destroyed. This is necessary to ensure that the
// compositor thread components of this class, i.e., the ui::InputHandlerProxy
// and any weak ptrs dispensed for posting tasks to the class on the compositor
// thread, are destroyed before the class is destroyed on the main thread.
class BlimpInputHandlerWrapper : public ui::InputHandlerProxyClient {
 public:
  BlimpInputHandlerWrapper(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      base::SingleThreadTaskRunner* compositor_task_runner,
      const base::WeakPtr<BlimpInputManager> input_manager_weak_ptr,
      const base::WeakPtr<cc::InputHandler>& input_handler_weak_ptr);

  ~BlimpInputHandlerWrapper() override;

  // Called by the BlimpInputManager to process a web gesture event. This will
  // call BlimpInputManager::HandleWebGestureEvent with the result on the main
  // thread.
  void HandleWebGestureEvent(const blink::WebGestureEvent& gesture_event);

  void InitOnCompositorThread(cc::InputHandler* input_handler);

 private:
  // InputHandlerProxyClient implementation.
  void WillShutdown() override;
  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) override;
  void DispatchNonBlockingEventToMainThread(
      ui::ScopedWebInputEvent event,
      const ui::LatencyInfo& latency_info) override;
  blink::WebGestureCurve* CreateFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;
  void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::Vector2dF& latest_overscroll_delta,
                     const gfx::Vector2dF& current_fling_velocity,
                     const gfx::PointF& causal_event_viewport_point) override;
  void DidStopFlinging() override;
  void DidAnimateForInput() override;

  base::ThreadChecker compositor_thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Used to queue calls to the BlimpInputManager to be run on the main
  // thread. This ensures that any tasks queued are abandoned after the
  // BlimpInputManager is destroyed.
  base::WeakPtr<BlimpInputManager> input_manager_weak_ptr_;

  std::unique_ptr<ui::InputHandlerProxy> input_handler_proxy_;

  // Used to dispense weak ptrs to post tasks to the wrapper on the compositor
  // thread. The weak ptrs created using this factory will be invalidated in
  // WillShutdown. This method is called on the compositor thread when the
  // InputHandlerProxy is being terminated. The wrapper does not need to be used
  // beyond this point.
  base::WeakPtrFactory<BlimpInputHandlerWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpInputHandlerWrapper);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_INPUT_BLIMP_INPUT_HANDLER_WRAPPER_H_
