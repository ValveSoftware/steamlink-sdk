// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/LockOrientationCallback.h"

#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/screen_orientation/ScreenOrientation.h"

namespace WebCore {

LockOrientationCallback::LockOrientationCallback(PassRefPtr<ScriptPromiseResolverWithContext> resolver)
    : m_resolver(resolver)
{
}

LockOrientationCallback::~LockOrientationCallback()
{
}

void LockOrientationCallback::onSuccess(unsigned angle, blink::WebScreenOrientationType type)
{
    // FIXME: for the moment, we do nothing with the angle, we should use it and
    // convert it to the appropriate type if the type == 'undefined' when the
    // method will be implemented in ScreenOrientationController.
    m_resolver->resolve(ScreenOrientation::orientationTypeToString(type));
}

void LockOrientationCallback::onError(ErrorType error)
{
    ExceptionCode code = 0;
    String msg = "";

    switch (error) {
    case ErrorTypeNotAvailable:
        code = NotSupportedError;
        msg = "lockOrientation() is not available on this device.";
        break;
    case ErrorTypeFullScreenRequired:
        code = SecurityError;
        msg = "The page needs to be fullscreen in order to call lockOrientation().";
        break;
    case ErrorTypeCanceled:
        code = AbortError;
        msg = "A call to lockOrientation() or unlockOrientation() canceled this call.";
        break;
    }

    m_resolver->reject(DOMException::create(code, msg));
}

void LockOrientationCallback::onError(blink::WebLockOrientationError error)
{
    ExceptionCode code = 0;
    String msg = "";

    switch (error) {
    case blink::WebLockOrientationErrorNotAvailable:
        code = NotSupportedError;
        msg = "lockOrientation() is not available on this device.";
        break;
    case blink::WebLockOrientationErrorFullScreenRequired:
        code = SecurityError;
        msg = "The page needs to be fullscreen in order to call lockOrientation().";
        break;
    case blink::WebLockOrientationErrorCanceled:
        code = AbortError;
        msg = "A call to lockOrientation() or unlockOrientation() canceled this call.";
        break;
    }

    m_resolver->reject(DOMException::create(code, msg));
}

} // namespace WebCore
