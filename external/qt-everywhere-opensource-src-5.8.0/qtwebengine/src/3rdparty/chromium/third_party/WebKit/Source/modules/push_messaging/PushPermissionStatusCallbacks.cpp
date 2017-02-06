// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushPermissionStatusCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/push_messaging/PushError.h"
#include "wtf/Assertions.h"
#include "wtf/text/WTFString.h"

namespace blink {

PushPermissionStatusCallbacks::PushPermissionStatusCallbacks(ScriptPromiseResolver* resolver)
    : m_resolver(resolver)
{
}

PushPermissionStatusCallbacks::~PushPermissionStatusCallbacks()
{
}

void PushPermissionStatusCallbacks::onSuccess(WebPushPermissionStatus status)
{
    m_resolver->resolve(permissionString(status));
}

void PushPermissionStatusCallbacks::onError(const WebPushError& error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject(PushError::take(m_resolver.get(), error));
}

// static
String PushPermissionStatusCallbacks::permissionString(WebPushPermissionStatus status)
{
    switch (status) {
    case WebPushPermissionStatusGranted:
        return "granted";
    case WebPushPermissionStatusDenied:
        return "denied";
    case WebPushPermissionStatusPrompt:
        return "prompt";
    }

    NOTREACHED();
    return "denied";
}

} // namespace blink
