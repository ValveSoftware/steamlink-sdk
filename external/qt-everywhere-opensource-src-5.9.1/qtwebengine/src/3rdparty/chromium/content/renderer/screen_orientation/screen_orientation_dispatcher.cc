// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include "content/common/screen_orientation_messages.h"

namespace content {

ScreenOrientationDispatcher::ScreenOrientationDispatcher(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

ScreenOrientationDispatcher::~ScreenOrientationDispatcher() {
}

bool ScreenOrientationDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ScreenOrientationDispatcher, message)
    IPC_MESSAGE_HANDLER(ScreenOrientationMsg_LockSuccess,
                        OnLockSuccess)
    IPC_MESSAGE_HANDLER(ScreenOrientationMsg_LockError,
                        OnLockError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ScreenOrientationDispatcher::OnDestruct() {
  delete this;
}

void ScreenOrientationDispatcher::OnLockSuccess(int request_id) {
  blink::WebLockOrientationCallback* callback =
      pending_callbacks_.Lookup(request_id);
  if (!callback)
    return;
  callback->onSuccess();
  pending_callbacks_.Remove(request_id);
}

void ScreenOrientationDispatcher::OnLockError(
    int request_id, blink::WebLockOrientationError error) {
  blink::WebLockOrientationCallback* callback =
      pending_callbacks_.Lookup(request_id);
  if (!callback)
    return;
  callback->onError(error);
  pending_callbacks_.Remove(request_id);
}

void ScreenOrientationDispatcher::CancelPendingLocks() {
  for (CallbackMap::Iterator<blink::WebLockOrientationCallback>
       iterator(&pending_callbacks_); !iterator.IsAtEnd(); iterator.Advance()) {
    iterator.GetCurrentValue()->onError(blink::WebLockOrientationErrorCanceled);
    pending_callbacks_.Remove(iterator.GetCurrentKey());
  }
}

void ScreenOrientationDispatcher::lockOrientation(
    blink::WebScreenOrientationLockType orientation,
    blink::WebLockOrientationCallback* callback) {
  CancelPendingLocks();

  int request_id = pending_callbacks_.Add(callback);
  Send(new ScreenOrientationHostMsg_LockRequest(
      routing_id(), orientation, request_id));
}

void ScreenOrientationDispatcher::unlockOrientation() {
  CancelPendingLocks();
  Send(new ScreenOrientationHostMsg_Unlock(routing_id()));
}

}  // namespace content
