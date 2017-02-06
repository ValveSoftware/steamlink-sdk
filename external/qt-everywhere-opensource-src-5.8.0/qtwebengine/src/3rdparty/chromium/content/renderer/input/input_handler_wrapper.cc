// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_wrapper.h"

#include "base/command_line.h"
#include "base/location.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager.h"
#include "third_party/WebKit/public/platform/Platform.h"

namespace content {

InputHandlerWrapper::InputHandlerWrapper(
    InputHandlerManager* input_handler_manager,
    int routing_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl,
    bool enable_smooth_scrolling)
    : input_handler_manager_(input_handler_manager),
      routing_id_(routing_id),
      input_handler_proxy_(input_handler.get(), this),
      main_task_runner_(main_task_runner),
      render_view_impl_(render_view_impl) {
  DCHECK(input_handler);
  input_handler_proxy_.set_smooth_scroll_enabled(enable_smooth_scrolling);
}

InputHandlerWrapper::~InputHandlerWrapper() {
}

void InputHandlerWrapper::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RenderViewImpl::TransferActiveWheelFlingAnimation,
                            render_view_impl_, params));
}

void InputHandlerWrapper::WillShutdown() {
  input_handler_manager_->RemoveInputHandler(routing_id_);
}

blink::WebGestureCurve* InputHandlerWrapper::CreateFlingAnimationCurve(
    blink::WebGestureDevice deviceSource,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  DidStartFlinging();
  return blink::Platform::current()->createFlingAnimationCurve(
      deviceSource, velocity, cumulative_scroll);
}

void InputHandlerWrapper::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity,
    const gfx::PointF& causal_event_viewport_point) {
  DidOverscrollParams params;
  params.accumulated_overscroll = accumulated_overscroll;
  params.latest_overscroll_delta = latest_overscroll_delta;
  params.current_fling_velocity = current_fling_velocity;
  params.causal_event_viewport_point = causal_event_viewport_point;
  input_handler_manager_->DidOverscroll(routing_id_, params);
}

void InputHandlerWrapper::DidStartFlinging() {
  input_handler_manager_->DidStartFlinging(routing_id_);
}

void InputHandlerWrapper::DidStopFlinging() {
  input_handler_manager_->DidStopFlinging(routing_id_);
}

void InputHandlerWrapper::DidAnimateForInput() {
  input_handler_manager_->DidAnimateForInput();
}

}  // namespace content
