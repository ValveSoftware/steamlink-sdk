// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionsCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/permissions/PermissionStatus.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PermissionsCallback::PermissionsCallback(ScriptPromiseResolver* resolver, std::unique_ptr<Vector<WebPermissionType>> internalPermissions, std::unique_ptr<Vector<int>> callerIndexToInternalIndex)
    : m_resolver(resolver)
    , m_internalPermissions(std::move(internalPermissions))
    , m_callerIndexToInternalIndex(std::move(callerIndexToInternalIndex))
{
    ASSERT(m_resolver);
}

void PermissionsCallback::onSuccess(std::unique_ptr<WebVector<WebPermissionStatus>> permissionStatus)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;

    std::unique_ptr<WebVector<WebPermissionStatus>> statusPtr = wrapUnique(permissionStatus.release());
    HeapVector<Member<PermissionStatus>> result(m_callerIndexToInternalIndex->size());

    // Create the response vector by finding the status for each index by
    // using the caller to internal index mapping and looking up the status
    // using the internal index obtained.
    for (size_t i = 0; i < m_callerIndexToInternalIndex->size(); ++i) {
        int internalIndex = m_callerIndexToInternalIndex->operator[](i);
        result[i] = PermissionStatus::createAndListen(m_resolver->getExecutionContext(), statusPtr->operator[](internalIndex), m_internalPermissions->operator[](internalIndex));
    }
    m_resolver->resolve(result);
}

void PermissionsCallback::onError()
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject();
}

} // namespace blink
