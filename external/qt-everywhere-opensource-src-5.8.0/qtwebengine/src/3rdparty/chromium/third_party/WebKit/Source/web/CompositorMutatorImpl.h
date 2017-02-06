// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutatorImpl_h
#define CompositorMutatorImpl_h

#include "core/animation/CustomCompositorAnimationManager.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class CompositorProxy;
class CompositorProxyClientImpl;
class CompositorWorkerGlobalScope;
class CompositorMutatorClient;

// Fans out requests from the compositor to all of the registered ProxyClients which
// can then mutate layers through their CompositorProxy interfaces. Requests for
// animation frames are received from ProxyClients and sent to the compositor to
// generate a new compositor frame.
//
// Should be accessed only on the compositor thread.
class CompositorMutatorImpl final : public CompositorMutator {
    WTF_MAKE_NONCOPYABLE(CompositorMutatorImpl);
public:
    static std::unique_ptr<CompositorMutatorClient> createClient();
    static CompositorMutatorImpl* create();

    DEFINE_INLINE_TRACE()
    {
        CompositorMutator::trace(visitor);
        visitor->trace(m_proxyClients);
    }

    // CompositorMutator implementation.
    bool mutate(double monotonicTimeNow, CompositorMutableStateProvider*) override;

    void registerProxyClient(CompositorProxyClientImpl*);

    void setNeedsMutate();

    void setClient(CompositorMutatorClient* client) { m_client = client; }
    CustomCompositorAnimationManager* animationManager() { return m_animationManager.get(); }

private:
    CompositorMutatorImpl();

    using ProxyClients = HeapHashSet<WeakMember<CompositorProxyClientImpl>>;
    ProxyClients m_proxyClients;

    std::unique_ptr<CustomCompositorAnimationManager> m_animationManager;
    CompositorMutatorClient* m_client;
};

} // namespace blink

#endif // CompositorMutatorImpl_h
