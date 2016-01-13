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

#ifndef ServiceWorkerContainer_h
#define ServiceWorkerContainer_h

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "public/platform/WebServiceWorkerProviderClient.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {
class WebServiceWorkerProvider;
class WebServiceWorker;
}

namespace WebCore {

class Dictionary;
class ExecutionContext;
class ServiceWorker;

class ServiceWorkerContainer FINAL :
    public RefCounted<ServiceWorkerContainer>,
    public ScriptWrappable,
    public ContextLifecycleObserver,
    public blink::WebServiceWorkerProviderClient {
public:
    static PassRefPtr<ServiceWorkerContainer> create(ExecutionContext*);
    ~ServiceWorkerContainer();

    void detachClient();

    PassRefPtrWillBeRawPtr<ServiceWorker> active() { return m_active.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> controller() { return m_controller.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> installing() { return m_installing.get(); }
    PassRefPtrWillBeRawPtr<ServiceWorker> waiting() { return m_waiting.get(); }
    ScriptPromise ready(ScriptState*);

    ScriptPromise registerServiceWorker(ScriptState*, const String& pattern, const Dictionary&);
    ScriptPromise unregisterServiceWorker(ScriptState*, const String& scope = String());

    // WebServiceWorkerProviderClient overrides.
    virtual void setActive(blink::WebServiceWorker*) OVERRIDE;
    virtual void setController(blink::WebServiceWorker*) OVERRIDE;
    virtual void setInstalling(blink::WebServiceWorker*) OVERRIDE;
    virtual void setWaiting(blink::WebServiceWorker*) OVERRIDE;
    virtual void dispatchMessageEvent(const blink::WebString& message, const blink::WebMessagePortChannelArray&) OVERRIDE;

private:
    explicit ServiceWorkerContainer(ExecutionContext*);

    blink::WebServiceWorkerProvider* m_provider;
    RefPtr<ServiceWorker> m_active;
    RefPtr<ServiceWorker> m_controller;
    RefPtr<ServiceWorker> m_installing;
    RefPtr<ServiceWorker> m_waiting;
};

} // namespace WebCore

#endif // ServiceWorkerContainer_h
