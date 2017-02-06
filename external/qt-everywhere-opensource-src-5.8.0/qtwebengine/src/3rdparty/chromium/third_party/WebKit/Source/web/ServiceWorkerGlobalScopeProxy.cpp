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

#include "web/ServiceWorkerGlobalScopeProxy.h"

#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "modules/background_sync/SyncEvent.h"
#include "modules/fetch/Headers.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationEvent.h"
#include "modules/notifications/NotificationEventInit.h"
#include "modules/push_messaging/PushEvent.h"
#include "modules/push_messaging/PushMessageData.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/ExtendableMessageEvent.h"
#include "modules/serviceworkers/FetchEvent.h"
#include "modules/serviceworkers/ForeignFetchEvent.h"
#include "modules/serviceworkers/InstallEvent.h"
#include "modules/serviceworkers/ServiceWorkerClient.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/web/WebSerializedScriptValue.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "web/WebEmbeddedWorkerImpl.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>
#include <utility>

namespace blink {

ServiceWorkerGlobalScopeProxy* ServiceWorkerGlobalScopeProxy::create(WebEmbeddedWorkerImpl& embeddedWorker, Document& document, WebServiceWorkerContextClient& client)
{
    return new ServiceWorkerGlobalScopeProxy(embeddedWorker, document, client);
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy()
{
    // Verify that the proxy has been detached.
    DCHECK(!m_embeddedWorker);
}

DEFINE_TRACE(ServiceWorkerGlobalScopeProxy)
{
    visitor->trace(m_document);
    visitor->trace(m_workerGlobalScope);
}

void ServiceWorkerGlobalScopeProxy::setRegistration(std::unique_ptr<WebServiceWorkerRegistration::Handle> handle)
{
    workerGlobalScope()->setRegistration(std::move(handle));
}

void ServiceWorkerGlobalScopeProxy::dispatchActivateEvent(int eventID)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Activate, eventID);
    Event* event = ExtendableEvent::create(EventTypeNames::activate, ExtendableEventInit(), observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchExtendableMessageEvent(int eventID, const WebString& message, const WebSecurityOrigin& sourceOrigin, const WebMessagePortChannelArray& webChannels, const WebServiceWorkerClientInfo& client)
{
    WebSerializedScriptValue value = WebSerializedScriptValue::fromString(message);
    MessagePortArray* ports = MessagePort::toMessagePortArray(m_workerGlobalScope, webChannels);
    String origin;
    if (!sourceOrigin.isUnique())
        origin = sourceOrigin.toString();
    ServiceWorkerClient* source = nullptr;
    if (client.clientType == WebServiceWorkerClientTypeWindow)
        source = ServiceWorkerWindowClient::create(client);
    else
        source = ServiceWorkerClient::create(client);
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Message, eventID);

    Event* event = ExtendableMessageEvent::create(value, origin, ports, source, observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchExtendableMessageEvent(int eventID, const WebString& message, const WebSecurityOrigin& sourceOrigin, const WebMessagePortChannelArray& webChannels, std::unique_ptr<WebServiceWorker::Handle> handle)
{
    WebSerializedScriptValue value = WebSerializedScriptValue::fromString(message);
    MessagePortArray* ports = MessagePort::toMessagePortArray(m_workerGlobalScope, webChannels);
    String origin;
    if (!sourceOrigin.isUnique())
        origin = sourceOrigin.toString();
    ServiceWorker* source = ServiceWorker::from(m_workerGlobalScope->getExecutionContext(), wrapUnique(handle.release()));
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Message, eventID);

    Event* event = ExtendableMessageEvent::create(value, origin, ports, source, observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchFetchEvent(int responseID, int eventFinishID, const WebServiceWorkerRequest& webRequest)
{
    ScriptState::Scope scope(workerGlobalScope()->scriptController()->getScriptState());
    WaitUntilObserver* waitUntilObserver = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Fetch, eventFinishID);
    RespondWithObserver* respondWithObserver = RespondWithObserver::create(workerGlobalScope(), responseID, webRequest.url(), webRequest.mode(), webRequest.frameType(), webRequest.requestContext(), waitUntilObserver);
    Request* request = Request::create(workerGlobalScope()->scriptController()->getScriptState(), webRequest);
    request->getHeaders()->setGuard(Headers::ImmutableGuard);
    FetchEventInit eventInit;
    eventInit.setCancelable(true);
    eventInit.setRequest(request);
    eventInit.setClientId(webRequest.isMainResourceLoad() ? WebString() : webRequest.clientId());
    eventInit.setIsReload(webRequest.isReload());
    FetchEvent* fetchEvent = FetchEvent::create(workerGlobalScope()->scriptController()->getScriptState(), EventTypeNames::fetch, eventInit, respondWithObserver, waitUntilObserver);
    waitUntilObserver->willDispatchEvent();
    DispatchEventResult dispatchResult = workerGlobalScope()->dispatchEvent(fetchEvent);
    respondWithObserver->didDispatchEvent(dispatchResult);
    // false is okay because waitUntil for fetch event doesn't care about the
    // promise rejection or an uncaught runtime script error.
    waitUntilObserver->didDispatchEvent(false /* errorOccurred */);
}

void ServiceWorkerGlobalScopeProxy::dispatchForeignFetchEvent(int responseID, int eventFinishID, const WebServiceWorkerRequest& webRequest)
{
    ScriptState::Scope scope(workerGlobalScope()->scriptController()->getScriptState());
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(webRequest.referrerUrl());
    WaitUntilObserver* waitUntilObserver = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Fetch, eventFinishID);
    ForeignFetchRespondWithObserver* respondWithObserver = ForeignFetchRespondWithObserver::create(workerGlobalScope(), responseID, webRequest.url(), webRequest.mode(), webRequest.frameType(), webRequest.requestContext(), origin, waitUntilObserver);
    Request* request = Request::create(workerGlobalScope()->scriptController()->getScriptState(), webRequest);
    request->getHeaders()->setGuard(Headers::ImmutableGuard);
    ForeignFetchEventInit eventInit;
    eventInit.setCancelable(true);
    eventInit.setRequest(request);
    eventInit.setOrigin(origin->toString());
    ForeignFetchEvent* fetchEvent = ForeignFetchEvent::create(workerGlobalScope()->scriptController()->getScriptState(), EventTypeNames::foreignfetch, eventInit, respondWithObserver, waitUntilObserver);
    waitUntilObserver->willDispatchEvent();
    DispatchEventResult dispatchResult = workerGlobalScope()->dispatchEvent(fetchEvent);
    respondWithObserver->didDispatchEvent(dispatchResult);
    // false is okay because waitUntil for foreign fetch event doesn't care
    // about the promise rejection or an uncaught runtime script error.
    waitUntilObserver->didDispatchEvent(false /* errorOccurred */);
}

void ServiceWorkerGlobalScopeProxy::dispatchInstallEvent(int eventID)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Install, eventID);
    Event* event;
    if (RuntimeEnabledFeatures::foreignFetchEnabled())
        event = InstallEvent::create(EventTypeNames::install, ExtendableEventInit(), observer);
    else
        event = ExtendableEvent::create(EventTypeNames::install, ExtendableEventInit(), observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchNotificationClickEvent(int eventID, int64_t notificationID, const WebNotificationData& data, int actionIndex)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::NotificationClick, eventID);
    NotificationEventInit eventInit;
    eventInit.setNotification(Notification::create(workerGlobalScope(), notificationID, data, true /* showing */));
    if (0 <= actionIndex && actionIndex < static_cast<int>(data.actions.size()))
        eventInit.setAction(data.actions[actionIndex].action);
    Event* event = NotificationEvent::create(EventTypeNames::notificationclick, eventInit, observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchNotificationCloseEvent(int eventID, int64_t notificationID, const WebNotificationData& data)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::NotificationClose, eventID);
    NotificationEventInit eventInit;
    eventInit.setAction(WTF::String()); // initialize as null.
    eventInit.setNotification(Notification::create(workerGlobalScope(), notificationID, data, false /* showing */));
    Event* event = NotificationEvent::create(EventTypeNames::notificationclose, eventInit, observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchPushEvent(int eventID, const WebString& data)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Push, eventID);
    Event* event = PushEvent::create(EventTypeNames::push, PushMessageData::create(data), observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchSyncEvent(int eventID, const WebString& tag, LastChanceOption lastChance)
{
    if (!RuntimeEnabledFeatures::backgroundSyncEnabled()) {
        ServiceWorkerGlobalScopeClient::from(workerGlobalScope())->didHandleSyncEvent(eventID, WebServiceWorkerEventResultCompleted);
        return;
    }
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Sync, eventID);
    Event* event = SyncEvent::create(EventTypeNames::sync, tag, lastChance == IsLastChance, observer);
    workerGlobalScope()->dispatchExtendableEvent(event, observer);
}

bool ServiceWorkerGlobalScopeProxy::hasFetchEventHandler()
{
    DCHECK(m_workerGlobalScope);
    return m_workerGlobalScope->hasEventListeners(EventTypeNames::fetch);
}

void ServiceWorkerGlobalScopeProxy::reportException(const String& errorMessage, std::unique_ptr<SourceLocation> location)
{
    client().reportException(errorMessage, location->lineNumber(), location->columnNumber(), location->url());
}

void ServiceWorkerGlobalScopeProxy::reportConsoleMessage(ConsoleMessage* consoleMessage)
{
    client().reportConsoleMessage(consoleMessage->source(), consoleMessage->level(), consoleMessage->message(), consoleMessage->location()->lineNumber(), consoleMessage->location()->url());
}

void ServiceWorkerGlobalScopeProxy::postMessageToPageInspector(const String& message)
{
    DCHECK(m_embeddedWorker);
    document().postInspectorTask(BLINK_FROM_HERE, createCrossThreadTask(&WebEmbeddedWorkerImpl::postMessageToPageInspector, crossThreadUnretained(m_embeddedWorker), message));
}

void ServiceWorkerGlobalScopeProxy::didEvaluateWorkerScript(bool success)
{
    client().didEvaluateWorkerScript(success);
}

void ServiceWorkerGlobalScopeProxy::didInitializeWorkerContext()
{
    ScriptState::Scope scope(workerGlobalScope()->scriptController()->getScriptState());
    client().didInitializeWorkerContext(workerGlobalScope()->scriptController()->context());
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeStarted(WorkerGlobalScope* workerGlobalScope)
{
    DCHECK(!m_workerGlobalScope);
    m_workerGlobalScope = static_cast<ServiceWorkerGlobalScope*>(workerGlobalScope);
    client().workerContextStarted(this);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeClosed()
{
    // This should never be called because close() is not defined in
    // ServiceWorkerGlobalScope.
    NOTREACHED();
}

void ServiceWorkerGlobalScopeProxy::willDestroyWorkerGlobalScope()
{
    v8::HandleScope handleScope(workerGlobalScope()->thread()->isolate());
    client().willDestroyWorkerContext(workerGlobalScope()->scriptController()->context());
    m_workerGlobalScope = nullptr;
}

void ServiceWorkerGlobalScopeProxy::workerThreadTerminated()
{
    client().workerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl& embeddedWorker, Document& document, WebServiceWorkerContextClient& client)
    : m_embeddedWorker(&embeddedWorker)
    , m_document(&document)
    , m_client(&client)
    , m_workerGlobalScope(nullptr)
{
}

void ServiceWorkerGlobalScopeProxy::detach()
{
    m_embeddedWorker = nullptr;
    m_document = nullptr;
    m_client = nullptr;
    m_workerGlobalScope = nullptr;
}

WebServiceWorkerContextClient& ServiceWorkerGlobalScopeProxy::client() const
{
    DCHECK(m_client);
    return *m_client;
}

Document& ServiceWorkerGlobalScopeProxy::document() const
{
    DCHECK(m_document);
    return *m_document;
}

ServiceWorkerGlobalScope* ServiceWorkerGlobalScopeProxy::workerGlobalScope() const
{
    DCHECK(m_workerGlobalScope);
    return m_workerGlobalScope;
}

} // namespace blink
