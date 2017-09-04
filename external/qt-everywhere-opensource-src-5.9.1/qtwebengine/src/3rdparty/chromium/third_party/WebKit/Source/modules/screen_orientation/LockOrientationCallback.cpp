// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/LockOrientationCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/screen_orientation/ScreenOrientation.h"

namespace blink {

LockOrientationCallback::LockOrientationCallback(
    ScriptPromiseResolver* resolver)
    : m_resolver(resolver) {}

LockOrientationCallback::~LockOrientationCallback() {}

void LockOrientationCallback::onSuccess() {
  m_resolver->resolve();
}

void LockOrientationCallback::onError(WebLockOrientationError error) {
  ExceptionCode code = 0;
  String msg = "";

  switch (error) {
    case WebLockOrientationErrorNotAvailable:
      code = NotSupportedError;
      msg = "screen.orientation.lock() is not available on this device.";
      break;
    case WebLockOrientationErrorFullscreenRequired:
      code = SecurityError;
      msg =
          "The page needs to be fullscreen in order to call "
          "screen.orientation.lock().";
      break;
    case WebLockOrientationErrorCanceled:
      code = AbortError;
      msg =
          "A call to screen.orientation.lock() or screen.orientation.unlock() "
          "canceled this call.";
      break;
  }

  m_resolver->reject(DOMException::create(code, msg));
}

}  // namespace blink
