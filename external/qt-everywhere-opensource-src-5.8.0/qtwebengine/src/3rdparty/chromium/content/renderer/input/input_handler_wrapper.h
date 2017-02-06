// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/input/input_handler_manager.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"

namespace ui {
class InputHandlerProxy;
class InputHandlerProxyClient;
}

namespace content {

// This class lives on the compositor thread.
class InputHandlerWrapper : public ui::InputHandlerProxyClient {
 public:
  InputHandlerWrapper(
      InputHandlerManager* input_handler_manager,
      int routing_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderViewImpl>& render_view_impl,
      bool enable_smooth_scrolling);
  ~InputHandlerWrapper() override;

  int routing_id() const { return routing_id_; }
  ui::InputHandlerProxy* input_handler_proxy() {
    return &input_handler_proxy_;
  }

  // InputHandlerProxyClient implementation.
  void WillShutdown() override;
  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) override;
  blink::WebGestureCurve* CreateFlingAnimationCurve(
      blink::WebGestureDevice deviceSource,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulativeScroll) override;
  void DidOverscroll(
        const gfx::Vector2dF& accumulated_overscroll,
        const gfx::Vector2dF& latest_overscroll_delta,
        const gfx::Vector2dF& current_fling_velocity,
        const gfx::PointF& causal_event_viewport_point) override;
  void DidStartFlinging() override;
  void DidStopFlinging() override;
  void DidAnimateForInput() override;

 private:
  InputHandlerManager* input_handler_manager_;
  int routing_id_;
  ui::InputHandlerProxy input_handler_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Can only be accessed on the main thread.
  base::WeakPtr<RenderViewImpl> render_view_impl_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_
