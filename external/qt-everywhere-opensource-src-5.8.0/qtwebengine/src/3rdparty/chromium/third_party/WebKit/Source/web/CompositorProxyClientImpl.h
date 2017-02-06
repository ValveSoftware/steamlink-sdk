// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxyClientImpl_h
#define CompositorProxyClientImpl_h

#include "core/dom/CompositorProxyClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CompositorMutableStateProvider;
class CompositorMutatorImpl;
class CompositorWorkerGlobalScope;
class WorkerGlobalScope;

// Mediates between one CompositorWorkerGlobalScope and the associated CompositorMutatorImpl.
// There is one CompositorProxyClientImpl per worker but there may be multiple for a given
// mutator, e.g. if a single document creates multiple CompositorWorker objects.
//
// Should be accessed only on the compositor thread.
class CompositorProxyClientImpl final : public GarbageCollected<CompositorProxyClientImpl>, public CompositorProxyClient {
    USING_GARBAGE_COLLECTED_MIXIN(CompositorProxyClientImpl);
    WTF_MAKE_NONCOPYABLE(CompositorProxyClientImpl);
public:
    CompositorProxyClientImpl(CompositorMutatorImpl*);
    DECLARE_VIRTUAL_TRACE();

    // Runs the animation frame callback for the frame starting at the given time.
    // Returns true if another animation frame was requested (i.e. should be reinvoked next frame).
    bool mutate(double monotonicTimeNow, CompositorMutableStateProvider*);

    // CompositorProxyClient:
    void setGlobalScope(WorkerGlobalScope*) override;
    void requestAnimationFrame() override;
    void registerCompositorProxy(CompositorProxy*) override;
    void unregisterCompositorProxy(CompositorProxy*) override;

private:
    bool executeAnimationFrameCallbacks(double monotonicTimeNow);

    Member<CompositorMutatorImpl> m_mutator;

    Member<CompositorWorkerGlobalScope> m_globalScope;
    bool m_requestedAnimationFrameCallbacks;

    HeapHashSet<WeakMember<CompositorProxy>> m_proxies;
};

} // namespace blink

#endif // CompositorProxyClientImpl_h
