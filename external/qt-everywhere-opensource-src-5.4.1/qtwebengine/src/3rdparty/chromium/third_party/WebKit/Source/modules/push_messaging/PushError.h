// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushError_h
#define PushError_h

#include "core/dom/DOMException.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebPushError.h"

namespace WebCore {

class ScriptPromiseResolverWithContext;

class PushError {
    WTF_MAKE_NONCOPYABLE(PushError);
public:
    // For CallbackPromiseAdapter.
    typedef blink::WebPushError WebType;
    static PassRefPtrWillBeRawPtr<DOMException> from(ScriptPromiseResolverWithContext*, WebType* webErrorRaw);

private:
    PushError() WTF_DELETED_FUNCTION;
};

} // namespace WebCore

#endif // PushError_h
