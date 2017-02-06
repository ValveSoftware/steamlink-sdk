// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/permissions/PermissionStatus.h"

namespace blink {

PermissionCallback::PermissionCallback(ScriptPromiseResolver* resolver, WebPermissionType permissionType)
    : m_resolver(resolver)
    , m_permissionType(permissionType)
{
    ASSERT(m_resolver);
}

PermissionCallback::~PermissionCallback()
{
}

void PermissionCallback::onSuccess(WebPermissionStatus permissionStatus)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->resolve(PermissionStatus::take(m_resolver.get(), permissionStatus, m_permissionType));
}

void PermissionCallback::onError()
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject();
}

} // namespace blink
