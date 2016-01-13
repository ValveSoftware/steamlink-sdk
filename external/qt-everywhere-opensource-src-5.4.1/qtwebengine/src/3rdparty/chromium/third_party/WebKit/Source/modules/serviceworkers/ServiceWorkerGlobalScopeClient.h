/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef ServiceWorkerGlobalScopeClient_h
#define ServiceWorkerGlobalScopeClient_h

#include "core/dom/MessagePort.h"
#include "core/workers/WorkerClients.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "public/platform/WebServiceWorkerEventResult.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {
class WebURL;
};

namespace WebCore {

class ExecutionContext;
class Response;
class WorkerClients;

class ServiceWorkerGlobalScopeClient : public WillBeHeapSupplement<WorkerClients> {
    WTF_MAKE_NONCOPYABLE(ServiceWorkerGlobalScopeClient);
public:
    virtual ~ServiceWorkerGlobalScopeClient() { }

    virtual void getClients(blink::WebServiceWorkerClientsCallbacks*) = 0;
    virtual blink::WebURL scope() const = 0;

    virtual void didHandleActivateEvent(int eventID, blink::WebServiceWorkerEventResult) = 0;
    virtual void didHandleInstallEvent(int installEventID, blink::WebServiceWorkerEventResult) = 0;
    // A null response means no valid response was provided by the service worker, so fallback to native.
    virtual void didHandleFetchEvent(int fetchEventID, PassRefPtr<Response> = nullptr) = 0;
    virtual void didHandleSyncEvent(int syncEventID) = 0;
    virtual void postMessageToClient(int clientID, const blink::WebString& message, PassOwnPtr<blink::WebMessagePortChannelArray>) = 0;

    static const char* supplementName();
    static ServiceWorkerGlobalScopeClient* from(ExecutionContext*);

protected:
    ServiceWorkerGlobalScopeClient() { }
};

void provideServiceWorkerGlobalScopeClientToWorker(WorkerClients*, PassOwnPtrWillBeRawPtr<ServiceWorkerGlobalScopeClient>);

} // namespace WebCore

#endif // ServiceWorkerGlobalScopeClient_h
