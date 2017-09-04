// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/input/blimp_input_handler_wrapper.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/input/blimp_input_manager.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

namespace blimp {
namespace client {
namespace {

void CreateProxyOnCompositorThread(
    BlimpInputHandlerWrapper* input_handler_wrapper,
    const base::WeakPtr<cc::InputHandler>& input_handler_weak_ptr) {
  if (!input_handler_weak_ptr)
    return;

  // If we have the input handler, the wrapper must be alive. So its safe to
  // access it.
  input_handler_wrapper->InitOnCompositorThread(input_handler_weak_ptr.get());
}

}  // namespace

BlimpInputHandlerWrapper::BlimpInputHandlerWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::SingleThreadTaskRunner* compositor_task_runner,
    const base::WeakPtr<BlimpInputManager> input_manager_weak_ptr,
    const base::WeakPtr<cc::InputHandler>& input_handler_weak_ptr)
    : main_task_runner_(main_task_runner),
      input_manager_weak_ptr_(input_manager_weak_ptr),
      weak_factory_(this) {
  DCHECK(main_task_runner->BelongsToCurrentThread());

  // Since the object is created on the main thread, detach the thread checker
  // so it can be bound to the compositor thread, in InitOnCompositorThread.
  compositor_thread_checker_.DetachFromThread();

  compositor_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&CreateProxyOnCompositorThread, this, input_handler_weak_ptr));
}

BlimpInputHandlerWrapper::~BlimpInputHandlerWrapper() = default;

void BlimpInputHandlerWrapper::HandleWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());

  // We might not have the input handler proxy anymore.
  if (!input_handler_proxy_)
    return;

  ui::InputHandlerProxy::EventDisposition disposition =
      input_handler_proxy_->HandleInputEvent(gesture_event);

  bool consumed = false;

  switch (disposition) {
    case ui::InputHandlerProxy::EventDisposition::DID_HANDLE:
    case ui::InputHandlerProxy::EventDisposition::DROP_EVENT:
      consumed = true;
      break;
    case ui::InputHandlerProxy::EventDisposition::DID_HANDLE_NON_BLOCKING:
    case ui::InputHandlerProxy::EventDisposition::DID_NOT_HANDLE:
      consumed = false;
      break;
    case ui::InputHandlerProxy::EventDisposition::
        DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING:
      NOTREACHED();
      break;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&BlimpInputManager::DidHandleWebGestureEvent,
                            input_manager_weak_ptr_, gesture_event, consumed));
}

void BlimpInputHandlerWrapper::WillShutdown() {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());

  input_handler_proxy_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void BlimpInputHandlerWrapper::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());

  NOTIMPLEMENTED()
      << "Transferring Fling Animations to the engine is not supported";
}

void BlimpInputHandlerWrapper::DispatchNonBlockingEventToMainThread(
    ui::ScopedWebInputEvent event,
    const ui::LatencyInfo& latency_info) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(event->type, blink::WebInputEvent::MouseWheel);

  NOTIMPLEMENTED()
      << "Dispatching Non Blocking Events to the engine is not supported";
}

blink::WebGestureCurve* BlimpInputHandlerWrapper::CreateFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());

  return ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
             gfx::Vector2dF(velocity.x, velocity.y),
             gfx::Vector2dF(cumulative_scroll.width, cumulative_scroll.height),
             false /* on_main_thread */)
      .release();
}

void BlimpInputHandlerWrapper::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity,
    const gfx::PointF& causal_event_viewport_point) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
}

void BlimpInputHandlerWrapper::DidStopFlinging() {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
}

void BlimpInputHandlerWrapper::DidAnimateForInput() {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());
}

void BlimpInputHandlerWrapper::InitOnCompositorThread(
    cc::InputHandler* input_handler) {
  DCHECK(compositor_thread_checker_.CalledOnValidThread());

  input_handler_proxy_ =
      base::MakeUnique<ui::InputHandlerProxy>(input_handler, this);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BlimpInputManager::OnInputHandlerWrapperInitialized,
                 input_manager_weak_ptr_, weak_factory_.GetWeakPtr()));
}

}  // namespace client
}  // namespace blimp
