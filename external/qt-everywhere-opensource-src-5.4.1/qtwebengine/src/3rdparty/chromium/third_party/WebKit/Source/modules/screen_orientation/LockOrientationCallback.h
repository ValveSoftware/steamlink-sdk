// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LockOrientationCallback_h
#define LockOrientationCallback_h

#include "public/platform/WebLockOrientationCallback.h"
#include "public/platform/WebScreenOrientationType.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class ScriptPromiseResolverWithContext;

// LockOrientationCallback is an implementation of WebLockOrientationCallback
// that will resolve the underlying promise depending on the result passed to
// the callback.
class LockOrientationCallback FINAL : public blink::WebLockOrientationCallback {
    WTF_MAKE_NONCOPYABLE(LockOrientationCallback);
public:
    explicit LockOrientationCallback(PassRefPtr<ScriptPromiseResolverWithContext>);
    virtual ~LockOrientationCallback();

    virtual void onSuccess(unsigned angle, blink::WebScreenOrientationType) OVERRIDE;
    virtual void onError(ErrorType) OVERRIDE;
    virtual void onError(blink::WebLockOrientationError) OVERRIDE;

private:
    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
};

} // namespace WebCore

#endif // LockOrientationCallback_h
