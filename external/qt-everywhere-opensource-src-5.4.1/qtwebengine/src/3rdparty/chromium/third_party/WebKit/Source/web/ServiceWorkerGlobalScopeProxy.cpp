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

#include "config.h"
#include "web/ServiceWorkerGlobalScopeProxy.h"

#include "bindings/v8/WorkerScriptController.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/events/MessageEvent.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/push_messaging/PushEvent.h"
#include "modules/serviceworkers/FetchEvent.h"
#include "modules/serviceworkers/InstallEvent.h"
#include "modules/serviceworkers/InstallPhaseEvent.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/NotImplemented.h"
#include "public/web/WebSerializedScriptValue.h"
#include "public/web/WebServiceWorkerContextClient.h"
#include "web/WebEmbeddedWorkerImpl.h"
#include "wtf/Functional.h"
#include "wtf/PassOwnPtr.h"

using namespace WebCore;

namespace blink {

PassOwnPtr<ServiceWorkerGlobalScopeProxy> ServiceWorkerGlobalScopeProxy::create(WebEmbeddedWorkerImpl& embeddedWorker, ExecutionContext& executionContext, WebServiceWorkerContextClient& client)
{
    return adoptPtr(new ServiceWorkerGlobalScopeProxy(embeddedWorker, executionContext, client));
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy()
{
}

void ServiceWorkerGlobalScopeProxy::dispatchInstallEvent(int eventID)
{
    ASSERT(m_workerGlobalScope);
    RefPtr<WaitUntilObserver> observer = WaitUntilObserver::create(m_workerGlobalScope, WaitUntilObserver::Install, eventID);
    observer->willDispatchEvent();
    m_workerGlobalScope->dispatchEvent(InstallEvent::create(EventTypeNames::install, EventInit(), observer));
    observer->didDispatchEvent();
}

void ServiceWorkerGlobalScopeProxy::dispatchActivateEvent(int eventID)
{
    ASSERT(m_workerGlobalScope);
    RefPtr<WaitUntilObserver> observer = WaitUntilObserver::create(m_workerGlobalScope, WaitUntilObserver::Activate, eventID);
    observer->willDispatchEvent();
    m_workerGlobalScope->dispatchEvent(InstallPhaseEvent::create(EventTypeNames::activate, EventInit(), observer));
    observer->didDispatchEvent();
}

void ServiceWorkerGlobalScopeProxy::dispatchFetchEvent(int eventID, const WebServiceWorkerRequest& webRequest)
{
    ASSERT(m_workerGlobalScope);
    RefPtr<RespondWithObserver> observer = RespondWithObserver::create(m_workerGlobalScope, eventID);
    RefPtr<Request> request = Request::create(webRequest);
    m_workerGlobalScope->dispatchEvent(FetchEvent::create(observer, request));
    observer->didDispatchEvent();
}

void ServiceWorkerGlobalScopeProxy::dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    ASSERT(m_workerGlobalScope);

    OwnPtr<MessagePortArray> ports = MessagePort::toMessagePortArray(m_workerGlobalScope, webChannels);
    WebSerializedScriptValue value = WebSerializedScriptValue::fromString(message);
    m_workerGlobalScope->dispatchEvent(MessageEvent::create(ports.release(), value));
}

void ServiceWorkerGlobalScopeProxy::dispatchPushEvent(int eventID, const WebString& data)
{
    ASSERT(m_workerGlobalScope);
    m_workerGlobalScope->dispatchEvent(PushEvent::create(EventTypeNames::push, data));
}

void ServiceWorkerGlobalScopeProxy::dispatchSyncEvent(int eventID)
{
    ASSERT(m_workerGlobalScope);
    m_workerGlobalScope->dispatchEvent(Event::create(EventTypeNames::sync));
    ServiceWorkerGlobalScopeClient::from(m_workerGlobalScope)->didHandleSyncEvent(eventID);
}

void ServiceWorkerGlobalScopeProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_client.reportException(errorMessage, lineNumber, columnNumber, sourceURL);
}

void ServiceWorkerGlobalScopeProxy::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    m_client.reportConsoleMessage(source, level, message, lineNumber, sourceURL);
}

void ServiceWorkerGlobalScopeProxy::postMessageToPageInspector(const String& message)
{
    m_client.dispatchDevToolsMessage(message);
}

void ServiceWorkerGlobalScopeProxy::updateInspectorStateCookie(const String& message)
{
    m_client.saveDevToolsAgentState(message);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeStarted(WorkerGlobalScope* workerGlobalScope)
{
    ASSERT(!m_workerGlobalScope);
    m_workerGlobalScope = workerGlobalScope;
    m_client.workerContextStarted(this);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeClosed()
{
    m_executionContext.postTask(bind(&WebEmbeddedWorkerImpl::terminateWorkerContext, &m_embeddedWorker));
}

void ServiceWorkerGlobalScopeProxy::willDestroyWorkerGlobalScope()
{
    m_workerGlobalScope = 0;
    m_client.willDestroyWorkerContext();
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeDestroyed()
{
    m_client.workerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl& embeddedWorker, ExecutionContext& executionContext, WebServiceWorkerContextClient& client)
    : m_embeddedWorker(embeddedWorker)
    , m_executionContext(executionContext)
    , m_client(client)
    , m_workerGlobalScope(0)
{
}

} // namespace blink
