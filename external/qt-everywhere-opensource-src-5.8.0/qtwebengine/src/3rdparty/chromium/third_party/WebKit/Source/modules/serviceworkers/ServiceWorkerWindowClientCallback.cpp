// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerWindowClientCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"
#include "wtf/PtrUtil.h"

namespace blink {

void NavigateClientCallback::onSuccess(std::unique_ptr<WebServiceWorkerClientInfo> clientInfo)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->resolve(ServiceWorkerWindowClient::take(m_resolver.get(), wrapUnique(clientInfo.release())));
}

void NavigateClientCallback::onError(const WebServiceWorkerError& error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;

    if (error.errorType == WebServiceWorkerError::ErrorTypeNavigation)  {
        ScriptState::Scope scope(m_resolver->getScriptState());
        m_resolver->reject(V8ThrowException::createTypeError(m_resolver->getScriptState()->isolate(), error.message));
        return;
    }

    m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
}

} // namespace blink
