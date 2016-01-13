/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ServiceWorkerGlobalScope_h
#define ServiceWorkerGlobalScope_h

#include "core/workers/WorkerGlobalScope.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"

namespace WebCore {

class FetchManager;
class Request;
class ScriptPromise;
class ScriptState;
class ServiceWorkerThread;
class ServiceWorkerClients;
class WorkerThreadStartupData;

class ServiceWorkerGlobalScope FINAL : public WorkerGlobalScope {
public:
    static PassRefPtrWillBeRawPtr<ServiceWorkerGlobalScope> create(ServiceWorkerThread*, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>);

    virtual ~ServiceWorkerGlobalScope();
    virtual bool isServiceWorkerGlobalScope() const OVERRIDE { return true; }
    virtual void stopFetch() OVERRIDE;

    // ServiceWorkerGlobalScope.idl
    PassRefPtr<ServiceWorkerClients> clients();
    String scope(ExecutionContext*);
    ScriptPromise fetch(ScriptState*, Request*);
    ScriptPromise fetch(ScriptState*, const String&);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(install);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(activate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(fetch);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(sync);

    virtual void trace(Visitor*) OVERRIDE;

private:
    ServiceWorkerGlobalScope(const KURL&, const String& userAgent, ServiceWorkerThread*, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerClients>);

    RefPtr<ServiceWorkerClients> m_clients;
    OwnPtr<FetchManager> m_fetchManager;
};

} // namespace WebCore

#endif // ServiceWorkerGlobalScope_h
