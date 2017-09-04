// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mouse_lock_dispatcher.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace content {

MouseLockDispatcher::MouseLockDispatcher() : mouse_locked_(false),
                                             pending_lock_request_(false),
                                             pending_unlock_request_(false),
                                             unlocked_by_target_(false),
                                             target_(NULL) {
}

MouseLockDispatcher::~MouseLockDispatcher() {
}

bool MouseLockDispatcher::LockMouse(LockTarget* target) {
  if (MouseLockedOrPendingAction())
    return false;

  pending_lock_request_ = true;
  target_ = target;

  SendLockMouseRequest(unlocked_by_target_);
  unlocked_by_target_ = false;
  return true;
}

void MouseLockDispatcher::UnlockMouse(LockTarget* target) {
  if (target && target == target_ && !pending_unlock_request_) {
    pending_unlock_request_ = true;

    // When a target application voluntarily unlocks the mouse we permit
    // relocking the mouse silently and with no user gesture requirement.
    // Check that the lock request is not currently pending and not yet
    // accepted by the browser process before setting |unlocked_by_target_|.
    if (!pending_lock_request_)
      unlocked_by_target_ = true;

    SendUnlockMouseRequest();
  }
}

void MouseLockDispatcher::OnLockTargetDestroyed(LockTarget* target) {
  if (target == target_) {
    UnlockMouse(target);
    target_ = NULL;
  }
}

bool MouseLockDispatcher::IsMouseLockedTo(LockTarget* target) {
  return mouse_locked_ && target_ == target;
}

bool MouseLockDispatcher::WillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (mouse_locked_ && target_)
    return target_->HandleMouseLockedInputEvent(event);
  return false;
}

void MouseLockDispatcher::OnLockMouseACK(bool succeeded) {
  DCHECK(!mouse_locked_ && pending_lock_request_);

  mouse_locked_ = succeeded;
  pending_lock_request_ = false;
  if (pending_unlock_request_ && !succeeded) {
    // We have sent an unlock request after the lock request. However, since
    // the lock request has failed, the unlock request will be ignored by the
    // browser side and there won't be any response to it.
    pending_unlock_request_ = false;
  }

  LockTarget* last_target = target_;
  if (!succeeded)
    target_ = NULL;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnLockMouseACK() synchronously calling LockMouse().

  if (last_target)
    last_target->OnLockMouseACK(succeeded);
}

void MouseLockDispatcher::OnMouseLockLost() {
  DCHECK(mouse_locked_ && !pending_lock_request_);

  mouse_locked_ = false;
  pending_unlock_request_ = false;

  LockTarget* last_target = target_;
  target_ = NULL;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnMouseLockLost() synchronously calling LockMouse().

  if (last_target)
    last_target->OnMouseLockLost();
}

}  // namespace content
