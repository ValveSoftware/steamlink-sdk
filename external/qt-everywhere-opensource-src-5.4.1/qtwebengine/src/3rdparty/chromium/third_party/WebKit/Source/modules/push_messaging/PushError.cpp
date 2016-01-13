// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushError.h"

#include "core/dom/ExceptionCode.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<DOMException> PushError::from(ScriptPromiseResolverWithContext*, WebType* webErrorRaw)
{
    OwnPtr<WebType> webError = adoptPtr(webErrorRaw);
    switch (webError->errorType) {
    case blink::WebPushError::ErrorTypeAbort:
        return DOMException::create(AbortError, "Registration failed.");
    case blink::WebPushError::ErrorTypeUnknown:
        return DOMException::create(UnknownError);
    }
    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

} // namespace WebCore
