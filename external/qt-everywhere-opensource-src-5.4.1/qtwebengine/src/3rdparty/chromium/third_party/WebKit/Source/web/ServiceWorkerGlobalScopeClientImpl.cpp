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

#include "config.h"
#include "web/ServiceWorkerGlobalScopeClientImpl.h"

#include "modules/serviceworkers/Response.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include "public/platform/WebURL.h"
#include "public/web/WebServiceWorkerContextClient.h"
#include "wtf/PassOwnPtr.h"

using namespace WebCore;

namespace blink {

PassOwnPtrWillBeRawPtr<ServiceWorkerGlobalScopeClient> ServiceWorkerGlobalScopeClientImpl::create(WebServiceWorkerContextClient& client)
{
    return adoptPtrWillBeNoop(new ServiceWorkerGlobalScopeClientImpl(client));
}

ServiceWorkerGlobalScopeClientImpl::~ServiceWorkerGlobalScopeClientImpl()
{
}

void ServiceWorkerGlobalScopeClientImpl::getClients(WebServiceWorkerClientsCallbacks* callbacks)
{
    m_client.getClients(callbacks);
}

WebURL ServiceWorkerGlobalScopeClientImpl::scope() const
{
    return m_client.scope();
}

void ServiceWorkerGlobalScopeClientImpl::didHandleActivateEvent(int eventID, WebServiceWorkerEventResult result)
{
    m_client.didHandleActivateEvent(eventID, result);
}

void ServiceWorkerGlobalScopeClientImpl::didHandleInstallEvent(int installEventID, WebServiceWorkerEventResult result)
{
    m_client.didHandleInstallEvent(installEventID, result);
}

void ServiceWorkerGlobalScopeClientImpl::didHandleFetchEvent(int fetchEventID, PassRefPtr<Response> response)
{
    if (!response) {
        m_client.didHandleFetchEvent(fetchEventID);
        return;
    }

    WebServiceWorkerResponse webResponse;
    response->populateWebServiceWorkerResponse(webResponse);
    m_client.didHandleFetchEvent(fetchEventID, webResponse);
}

void ServiceWorkerGlobalScopeClientImpl::didHandleSyncEvent(int syncEventID)
{
    m_client.didHandleSyncEvent(syncEventID);
}

void ServiceWorkerGlobalScopeClientImpl::postMessageToClient(int clientID, const WebString& message, PassOwnPtr<WebMessagePortChannelArray> webChannels)
{
    m_client.postMessageToClient(clientID, message, webChannels.leakPtr());
}

ServiceWorkerGlobalScopeClientImpl::ServiceWorkerGlobalScopeClientImpl(WebServiceWorkerContextClient& client)
    : m_client(client)
{
}

} // namespace blink
