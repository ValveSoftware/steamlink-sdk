// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/input/blimp_input_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/input/blimp_input_handler_wrapper.h"
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
                            ui::GestureProviderConfigType::CURRENT_PLATFORM),
                        this),
      compositor_task_runner_(std::move(compositor_task_runner)),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  input_handler_wrapper_ = base::MakeUnique<BlimpInputHandlerWrapper>(
      main_task_runner, compositor_task_runner_.get(),
      weak_factory_.GetWeakPtr(), input_handler);
}

BlimpInputManager::~BlimpInputManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool BlimpInputManager::OnTouchEvent(const ui::MotionEvent& motion_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(motion_event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent touch = ui::CreateWebTouchEventFromMotionEvent(
      motion_event, result.moved_beyond_slop_region);

  // Touch events are queued in the Gesture Provider until acknowledged to
  // allow them to be consumed by the touch event handlers in blink which can
  // prevent-default on the event. Since we currently do not support touch
  // handlers the event is always acknowledged as not consumed.
  gesture_provider_.OnTouchEventAck(touch.uniqueTouchEventId, false);

  return true;
}

void BlimpInputManager::OnInputHandlerWrapperInitialized(
    base::WeakPtr<BlimpInputHandlerWrapper> input_handler_wrapper_weak_ptr) {
  DCHECK(thread_checker_.CalledOnValidThread());
  input_handler_wrapper_weak_ptr_ = input_handler_wrapper_weak_ptr;
}

void BlimpInputManager::OnGestureEvent(const ui::GestureEventData& gesture) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
      FROM_HERE, base::Bind(&BlimpInputHandlerWrapper::HandleWebGestureEvent,
                            input_handler_wrapper_weak_ptr_, web_gesture));
}

void BlimpInputManager::DidHandleWebGestureEvent(
    const blink::WebGestureEvent& gesture_event,
    bool consumed) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!consumed)
    client_->SendWebGestureEvent(gesture_event);
}

}  // namespace client
}  // namespace blimp
