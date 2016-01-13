// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerContainerClient_h
#define ServiceWorkerContainerClient_h

#include "core/dom/DocumentSupplementable.h"
#include "core/workers/WorkerClients.h"
#include "wtf/Forward.h"

namespace blink {
class WebServiceWorkerProvider;
}

namespace WebCore {

class ExecutionContext;

// This mainly exists to provide access to WebServiceWorkerProvider.
// Owned by Document (or WorkerClients).
class ServiceWorkerContainerClient FINAL :
    public NoBaseWillBeGarbageCollectedFinalized<ServiceWorkerContainerClient>,
    public DocumentSupplement,
    public WillBeHeapSupplement<WorkerClients> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerContainerClient);
    WTF_MAKE_NONCOPYABLE(ServiceWorkerContainerClient);
public:
    static PassOwnPtrWillBeRawPtr<ServiceWorkerContainerClient> create(PassOwnPtr<blink::WebServiceWorkerProvider>);
    virtual ~ServiceWorkerContainerClient();

    blink::WebServiceWorkerProvider* provider() { return m_provider.get(); }

    static const char* supplementName();
    static ServiceWorkerContainerClient* from(ExecutionContext*);

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        DocumentSupplement::trace(visitor);
        WillBeHeapSupplement<WorkerClients>::trace(visitor);
    }

protected:
    explicit ServiceWorkerContainerClient(PassOwnPtr<blink::WebServiceWorkerProvider>);

    OwnPtr<blink::WebServiceWorkerProvider> m_provider;
};

void provideServiceWorkerContainerClientToWorker(WorkerClients*, PassOwnPtr<blink::WebServiceWorkerProvider>);

} // namespace WebCore

#endif // ServiceWorkerContainerClient_h
