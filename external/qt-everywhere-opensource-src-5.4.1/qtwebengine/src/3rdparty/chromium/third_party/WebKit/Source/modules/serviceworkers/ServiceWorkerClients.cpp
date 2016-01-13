// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"

#include "bindings/v8/CallbackPromiseAdapter.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "modules/serviceworkers/Client.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

namespace {

    class ClientArray {
    public:
        typedef blink::WebServiceWorkerClientsInfo WebType;
        static Vector<RefPtr<Client> > from(ScriptPromiseResolverWithContext*, WebType* webClientsRaw)
        {
            OwnPtr<WebType> webClients = adoptPtr(webClientsRaw);
            Vector<RefPtr<Client> > clients;
            for (size_t i = 0; i < webClients->clientIDs.size(); ++i) {
                clients.append(Client::create(webClients->clientIDs[i]));
            }
            return clients;
        }

    private:
        WTF_MAKE_NONCOPYABLE(ClientArray);
        ClientArray() WTF_DELETED_FUNCTION;
    };

} // namespace

PassRefPtr<ServiceWorkerClients> ServiceWorkerClients::create()
{
    return adoptRef(new ServiceWorkerClients());
}

ServiceWorkerClients::ServiceWorkerClients()
{
    ScriptWrappable::init(this);
}

ServiceWorkerClients::~ServiceWorkerClients()
{
}

ScriptPromise ServiceWorkerClients::getServiced(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolverWithContext> resolver = ScriptPromiseResolverWithContext::create(scriptState);
    ServiceWorkerGlobalScopeClient::from(scriptState->executionContext())->getClients(new CallbackPromiseAdapter<ClientArray, ServiceWorkerError>(resolver));
    return resolver->promise();
}

} // namespace WebCore
