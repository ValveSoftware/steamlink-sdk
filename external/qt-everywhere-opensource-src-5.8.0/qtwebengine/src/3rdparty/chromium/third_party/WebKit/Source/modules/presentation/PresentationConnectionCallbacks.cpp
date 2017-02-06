// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"
#include "public/platform/modules/presentation/WebPresentationConnectionClient.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PresentationConnectionCallbacks::PresentationConnectionCallbacks(ScriptPromiseResolver* resolver, PresentationRequest* request)
    : m_resolver(resolver)
    , m_request(request)
{
    ASSERT(m_resolver);
    ASSERT(m_request);
}

void PresentationConnectionCallbacks::onSuccess(std::unique_ptr<WebPresentationConnectionClient> PresentationConnectionClient)
{
    std::unique_ptr<WebPresentationConnectionClient> result(wrapUnique(PresentationConnectionClient.release()));

    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->resolve(PresentationConnection::take(m_resolver.get(), std::move(result), m_request));
}

void PresentationConnectionCallbacks::onError(const WebPresentationError& error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject(PresentationError::take(m_resolver.get(), error));
}

} // namespace blink
