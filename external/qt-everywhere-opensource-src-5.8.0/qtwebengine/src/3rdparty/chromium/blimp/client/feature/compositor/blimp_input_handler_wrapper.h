// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_INPUT_HANDLER_WRAPPER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_INPUT_HANDLER_WRAPPER_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"

namespace blimp {
namespace client {

class BlimpInputManager;

// The BlimpInputHandlerWrapper isolates all input handling processing done on
// the compositor thread from the BlimpInputManager. It takes web gesture events
// from the BlimpInputManager and sends them to the ui::InputHandlerProxy.
// The class is created and lives on the compositor thread.
class BlimpInputHandlerWrapper : public ui::InputHandlerProxyClient {
 public:
  BlimpInputHandlerWrapper(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const base::WeakPtr<BlimpInputManager> input_manager_weak_ptr,
      cc::InputHandler* input_handler);

  ~BlimpInputHandlerWrapper() override;

  // Called by the BlimpInputManager to process a web gesture event. This will
  // call BlimpInputManager::HandleWebGestureEvent with the result on the main
  // thread.
  void HandleWebGestureEvent(const blink::WebGestureEvent& gesture_event);

 private:
  // InputHandlerProxyClient implementation.
  void WillShutdown() override;
  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) override;
  blink::WebGestureCurve* CreateFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;
  void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::Vector2dF& latest_overscroll_delta,
                     const gfx::Vector2dF& current_fling_velocity,
                     const gfx::PointF& causal_event_viewport_point) override;
  void DidStartFlinging() override;
  void DidStopFlinging() override;
  void DidAnimateForInput() override;

  base::ThreadChecker compositor_thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Used to queue calls to the BlimpInputManager to be run on the main
  // thread. This ensures that any tasks queued are abandoned after the
  // BlimpInputManager is destroyed.
  base::WeakPtr<BlimpInputManager> input_manager_weak_ptr_;

  std::unique_ptr<ui::InputHandlerProxy> input_handler_proxy_;

  DISALLOW_COPY_AND_ASSIGN(BlimpInputHandlerWrapper);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_INPUT_HANDLER_WRAPPER_H_
