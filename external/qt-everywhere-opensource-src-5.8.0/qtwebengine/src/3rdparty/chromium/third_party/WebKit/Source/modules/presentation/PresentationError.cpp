// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

// static
DOMException* PresentationError::take(ScriptPromiseResolver*, const WebPresentationError& error)
{
    ExceptionCode code = UnknownError;
    switch (error.errorType) {
    case WebPresentationError::ErrorTypeNoAvailableScreens:
    case WebPresentationError::ErrorTypeNoPresentationFound:
        code = NotFoundError;
        break;
    case WebPresentationError::ErrorTypeSessionRequestCancelled:
        code = AbortError;
        break;
    case WebPresentationError::ErrorTypeAvailabilityNotSupported:
        code = NotSupportedError;
        break;
    case WebPresentationError::ErrorTypeUnknown:
        code = UnknownError;
        break;
    }

    return DOMException::create(code, error.message);
}

} // namespace blink
