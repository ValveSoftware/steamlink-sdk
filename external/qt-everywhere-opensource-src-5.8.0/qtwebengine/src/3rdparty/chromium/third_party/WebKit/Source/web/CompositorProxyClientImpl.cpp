// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/CompositorProxyClientImpl.h"

#include "core/dom/CompositorProxy.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/CompositorMutableStateProvider.h"
#include "web/CompositorMutatorImpl.h"
#include "wtf/CurrentTime.h"

namespace blink {

// A helper class that updates proxies mutable state on creation and reset it
// on destruction. This can be used with rAF callback to ensure no mutation is
// allowed outside rAF.
class ScopedCompositorMutableState final {
    WTF_MAKE_NONCOPYABLE(ScopedCompositorMutableState);
    STACK_ALLOCATED();
public:
    ScopedCompositorMutableState(HeapHashSet<WeakMember<CompositorProxy>>& proxies, CompositorMutableStateProvider* stateProvider)
        : m_proxies(proxies)
    {
        for (CompositorProxy* proxy : m_proxies)
            proxy->takeCompositorMutableState(stateProvider->getMutableStateFor(proxy->elementId()));

    }
    ~ScopedCompositorMutableState()
    {
        for (CompositorProxy* proxy : m_proxies)
            proxy->takeCompositorMutableState(nullptr);
    }
private:
    HeapHashSet<WeakMember<CompositorProxy>>& m_proxies;
};

CompositorProxyClientImpl::CompositorProxyClientImpl(CompositorMutatorImpl* mutator)
    : m_mutator(mutator)
    , m_globalScope(nullptr)
{
}

DEFINE_TRACE(CompositorProxyClientImpl)
{
    CompositorProxyClient::trace(visitor);
    visitor->trace(m_mutator);
    visitor->trace(m_globalScope);
    visitor->trace(m_proxies);
}

void CompositorProxyClientImpl::setGlobalScope(WorkerGlobalScope* scope)
{
    TRACE_EVENT0("compositor-worker", "CompositorProxyClientImpl::setGlobalScope");
    DCHECK(!m_globalScope);
    DCHECK(scope);
    m_globalScope = static_cast<CompositorWorkerGlobalScope*>(scope);
    m_mutator->registerProxyClient(this);
}

void CompositorProxyClientImpl::requestAnimationFrame()
{
    TRACE_EVENT0("compositor-worker", "CompositorProxyClientImpl::requestAnimationFrame");
    m_requestedAnimationFrameCallbacks = true;
    m_mutator->setNeedsMutate();
}

bool CompositorProxyClientImpl::mutate(double monotonicTimeNow, CompositorMutableStateProvider* stateProvider)
{
    if (!m_globalScope)
        return false;

    TRACE_EVENT0("compositor-worker", "CompositorProxyClientImpl::mutate");
    if (!m_requestedAnimationFrameCallbacks)
        return false;

    {
        ScopedCompositorMutableState mutableState(m_proxies, stateProvider);
        m_requestedAnimationFrameCallbacks = executeAnimationFrameCallbacks(monotonicTimeNow);
    }

    return m_requestedAnimationFrameCallbacks;
}

bool CompositorProxyClientImpl::executeAnimationFrameCallbacks(double monotonicTimeNow)
{
    TRACE_EVENT0("compositor-worker", "CompositorProxyClientImpl::executeAnimationFrameCallbacks");

    // Convert to zero based document time in milliseconds consistent with requestAnimationFrame.
    double highResTimeMs = 1000.0 * (monotonicTimeNow - m_globalScope->timeOrigin());
    return m_globalScope->executeAnimationFrameCallbacks(highResTimeMs);
}

void CompositorProxyClientImpl::registerCompositorProxy(CompositorProxy* proxy)
{
    m_proxies.add(proxy);
}

void CompositorProxyClientImpl::unregisterCompositorProxy(CompositorProxy* proxy)
{
    m_proxies.remove(proxy);
}

} // namespace blink
