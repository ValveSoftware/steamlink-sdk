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

#ifndef ServiceWorkerGlobalScopeClientImpl_h
#define ServiceWorkerGlobalScopeClientImpl_h

#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/OwnPtr.h"

namespace blink {

class WebServiceWorkerContextClient;
class WebURL;

class ServiceWorkerGlobalScopeClientImpl FINAL : public NoBaseWillBeGarbageCollectedFinalized<ServiceWorkerGlobalScopeClientImpl>, public WebCore::ServiceWorkerGlobalScopeClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerGlobalScopeClientImpl);
public:
    static PassOwnPtrWillBeRawPtr<WebCore::ServiceWorkerGlobalScopeClient> create(WebServiceWorkerContextClient&);
    virtual ~ServiceWorkerGlobalScopeClientImpl();

    virtual void getClients(WebServiceWorkerClientsCallbacks*);
    virtual WebURL scope() const OVERRIDE;

    virtual void didHandleActivateEvent(int eventID, WebServiceWorkerEventResult) OVERRIDE;
    virtual void didHandleInstallEvent(int installEventID, WebServiceWorkerEventResult) OVERRIDE;
    virtual void didHandleFetchEvent(int fetchEventID, PassRefPtr<WebCore::Response>) OVERRIDE;
    virtual void didHandleSyncEvent(int syncEventID) OVERRIDE;
    virtual void postMessageToClient(int clientID, const WebString& message, PassOwnPtr<WebMessagePortChannelArray>) OVERRIDE;

    virtual void trace(WebCore::Visitor* visitor) OVERRIDE { WebCore::ServiceWorkerGlobalScopeClient::trace(visitor); }

private:
    ServiceWorkerGlobalScopeClientImpl(WebServiceWorkerContextClient&);

    WebServiceWorkerContextClient& m_client;
};

} // namespace blink

#endif // ServiceWorkerGlobalScopeClientImpl_h
