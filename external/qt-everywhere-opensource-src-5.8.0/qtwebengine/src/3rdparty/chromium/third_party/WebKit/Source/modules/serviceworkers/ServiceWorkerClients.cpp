// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerClients.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLocation.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClientCallback.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientQueryOptions.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

namespace {

class ClientArray {
public:
    using WebType = const WebServiceWorkerClientsInfo&;
    static HeapVector<Member<ServiceWorkerClient>> take(ScriptPromiseResolver*, const WebServiceWorkerClientsInfo& webClients)
    {
        HeapVector<Member<ServiceWorkerClient>> clients;
        for (size_t i = 0; i < webClients.clients.size(); ++i) {
            const WebServiceWorkerClientInfo& client = webClients.clients[i];
            if (client.clientType == WebServiceWorkerClientTypeWindow)
                clients.append(ServiceWorkerWindowClient::create(client));
            else
                clients.append(ServiceWorkerClient::create(client));
        }
        return clients;
    }

private:
    WTF_MAKE_NONCOPYABLE(ClientArray);
    ClientArray() = delete;
};

WebServiceWorkerClientType getClientType(const String& type)
{
    if (type == "window")
        return WebServiceWorkerClientTypeWindow;
    if (type == "worker")
        return WebServiceWorkerClientTypeWorker;
    if (type == "sharedworker")
        return WebServiceWorkerClientTypeSharedWorker;
    if (type == "all")
        return WebServiceWorkerClientTypeAll;
    ASSERT_NOT_REACHED();
    return WebServiceWorkerClientTypeWindow;
}

class GetCallback : public WebServiceWorkerClientCallbacks {
public:
    explicit GetCallback(ScriptPromiseResolver* resolver)
        : m_resolver(resolver) { }
    ~GetCallback() override { }

    void onSuccess(std::unique_ptr<WebServiceWorkerClientInfo> webClient) override
    {
        std::unique_ptr<WebServiceWorkerClientInfo> client = wrapUnique(webClient.release());
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        if (!client) {
            // Resolve the promise with undefined.
            m_resolver->resolve();
            return;
        }
        m_resolver->resolve(ServiceWorkerClient::take(m_resolver, std::move(client)));
    }

    void onError(const WebServiceWorkerError& error) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(ServiceWorkerError::take(m_resolver.get(), error));
    }

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    WTF_MAKE_NONCOPYABLE(GetCallback);
};

} // namespace

ServiceWorkerClients* ServiceWorkerClients::create()
{
    return new ServiceWorkerClients();
}

ServiceWorkerClients::ServiceWorkerClients()
{
}

ScriptPromise ServiceWorkerClients::get(ScriptState* scriptState, const String& id)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    // TODO(jungkees): May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    ServiceWorkerGlobalScopeClient::from(executionContext)->getClient(id, new GetCallback(resolver));
    return promise;
}

ScriptPromise ServiceWorkerClients::matchAll(ScriptState* scriptState, const ClientQueryOptions& options)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    // FIXME: May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebServiceWorkerClientQueryOptions webOptions;
    webOptions.clientType = getClientType(options.type());
    webOptions.includeUncontrolled = options.includeUncontrolled();
    ServiceWorkerGlobalScopeClient::from(executionContext)->getClients(webOptions, new CallbackPromiseAdapter<ClientArray, ServiceWorkerError>(resolver));
    return promise;
}

ScriptPromise ServiceWorkerClients::claim(ScriptState* scriptState)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();

    // FIXME: May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebServiceWorkerClientsClaimCallbacks* callbacks = new CallbackPromiseAdapter<void, ServiceWorkerError>(resolver);
    ServiceWorkerGlobalScopeClient::from(executionContext)->claim(callbacks);
    return promise;
}

ScriptPromise ServiceWorkerClients::openWindow(ScriptState* scriptState, const String& url)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    ExecutionContext* context = scriptState->getExecutionContext();

    KURL parsedUrl = KURL(toWorkerGlobalScope(context)->location()->url(), url);
    if (!parsedUrl.isValid()) {
        resolver->reject(V8ThrowException::createTypeError(scriptState->isolate(), "'" + url + "' is not a valid URL."));
        return promise;
    }

    if (!context->getSecurityOrigin()->canDisplay(parsedUrl)) {
        resolver->reject(V8ThrowException::createTypeError(scriptState->isolate(), "'" + parsedUrl.elidedString() + "' cannot be opened."));
        return promise;
    }

    if (!context->isWindowInteractionAllowed()) {
        resolver->reject(DOMException::create(InvalidAccessError, "Not allowed to open a window."));
        return promise;
    }
    context->consumeWindowInteraction();

    ServiceWorkerGlobalScopeClient::from(context)->openWindow(parsedUrl, new NavigateClientCallback(resolver));
    return promise;
}

} // namespace blink
