// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailabilityCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationError.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

PresentationAvailabilityCallbacks::PresentationAvailabilityCallbacks(ScriptPromiseResolver* resolver, const KURL& url)
    : m_resolver(resolver)
    , m_url(url)
{
    ASSERT(m_resolver);
}

PresentationAvailabilityCallbacks::~PresentationAvailabilityCallbacks()
{
}

void PresentationAvailabilityCallbacks::onSuccess(bool value)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->resolve(PresentationAvailability::take(m_resolver.get(), m_url, value));
}

void PresentationAvailabilityCallbacks::onError(const WebPresentationError& error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject(PresentationError::take(m_resolver.get(), error));
}

} // namespace blink
