// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_input_manager.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace blimp {
namespace client {

std::unique_ptr<BlimpInputManager> BlimpInputManager::Create(
    BlimpInputManagerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    const base::WeakPtr<cc::InputHandler>& input_handler) {
  return base::WrapUnique(new BlimpInputManager(
      client, main_task_runner, compositor_task_runner, input_handler));
}

BlimpInputManager::BlimpInputManager(
    BlimpInputManagerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    const base::WeakPtr<cc::InputHandler>& input_handler)
    : client_(client),
      gesture_provider_(ui::GetGestureProviderConfig(
                ui::GestureProviderConfigType::CURRENT_PLATFORM), this),
      main_task_runner_(main_task_runner),
      compositor_task_runner_(compositor_task_runner),
      main_thread_blocked_(false),
      weak_factory_(this) {
  DCHECK(IsMainThread());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BlimpInputManager::CreateInputHandlerWrapperOnCompositorThread,
          base::Unretained(this), weak_factory_.GetWeakPtr(),
          input_handler));
}

BlimpInputManager::~BlimpInputManager() {
  DCHECK(IsMainThread());

  base::WaitableEvent shutdown_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  {
    base::AutoReset<bool> auto_reset_main_thread_blocked(
        &main_thread_blocked_, true);
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &BlimpInputManager::ShutdownOnCompositorThread,
            base::Unretained(this), &shutdown_event));
    shutdown_event.Wait();
  }
}

bool BlimpInputManager::OnTouchEvent(const ui::MotionEvent& motion_event) {
  DCHECK(IsMainThread());

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(motion_event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent touch =
      ui::CreateWebTouchEventFromMotionEvent(motion_event,
                                             result.moved_beyond_slop_region);

  // Touch events are queued in the Gesture Provider until acknowledged to
  // allow them to be consumed by the touch event handlers in blink which can
  // prevent-default on the event. Since we currently do not support touch
  // handlers the event is always acknowledged as not consumed.
  gesture_provider_.OnTouchEventAck(touch.uniqueTouchEventId, false);

  return true;
}

void BlimpInputManager::OnGestureEvent(const ui::GestureEventData& gesture) {
  DCHECK(IsMainThread());

  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(khushalsagar): Remove this workaround after Android fixes UiAutomator
  // to stop providing shift meta values to synthetic MotionEvents. This
  // prevents unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.type == blink::WebInputEvent::GestureTap &&
      web_gesture.modifiers == blink::WebInputEvent::ShiftKey) {
    web_gesture.modifiers = 0;
  }

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BlimpInputManager::HandleWebGestureEventOnCompositorThread,
                 base::Unretained(this), web_gesture));
}

void BlimpInputManager::CreateInputHandlerWrapperOnCompositorThread(
    base::WeakPtr<BlimpInputManager> input_manager_weak_ptr,
    const base::WeakPtr<cc::InputHandler>& input_handler) {
  DCHECK(IsCompositorThread());

  // The input_handler might have been destroyed at this point.
  if (!input_handler)
    return;

  DCHECK(!input_handler_wrapper_);
  input_handler_wrapper_ = base::WrapUnique(new BlimpInputHandlerWrapper(
      main_task_runner_, input_manager_weak_ptr, input_handler.get()));
}

void BlimpInputManager::HandleWebGestureEventOnCompositorThread(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK(IsCompositorThread());

  if (input_handler_wrapper_)
    input_handler_wrapper_->HandleWebGestureEvent(gesture_event);
}

void BlimpInputManager::ShutdownOnCompositorThread(
    base::WaitableEvent* shutdown_event) {
  DCHECK(IsCompositorThread());
  DCHECK(main_thread_blocked_);

  input_handler_wrapper_.reset();
  shutdown_event->Signal();
}

void BlimpInputManager::DidHandleWebGestureEvent(
    const blink::WebGestureEvent& gesture_event,
    bool consumed) {
  DCHECK(IsMainThread());

  if (!consumed)
    client_->SendWebGestureEvent(gesture_event);
}

bool BlimpInputManager::IsMainThread() const {
  return main_task_runner_->BelongsToCurrentThread();
}

bool BlimpInputManager::IsCompositorThread() const {
  return compositor_task_runner_->BelongsToCurrentThread();
}

}  // namespace client
}  // namespace blimp
