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

#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/ThreadableLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/EventTargetModules.h"
#include "modules/cachestorage/CacheStorage.h"
#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "modules/fetch/GlobalFetch.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "modules/serviceworkers/ServiceWorkerScriptCachedMetadataHandler.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/Histogram.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ServiceWorkerGlobalScope* ServiceWorkerGlobalScope::create(ServiceWorkerThread* thread, std::unique_ptr<WorkerThreadStartupData> startupData)
{
    // Note: startupData is finalized on return. After the relevant parts has been
    // passed along to the created 'context'.
    ServiceWorkerGlobalScope* context = new ServiceWorkerGlobalScope(startupData->m_scriptURL, startupData->m_userAgent, thread, monotonicallyIncreasingTime(), std::move(startupData->m_starterOriginPrivilegeData), startupData->m_workerClients.release());

    context->setV8CacheOptions(startupData->m_v8CacheOptions);
    context->applyContentSecurityPolicyFromVector(*startupData->m_contentSecurityPolicyHeaders);
    if (!startupData->m_referrerPolicy.isNull())
        context->parseAndSetReferrerPolicy(startupData->m_referrerPolicy);
    context->setAddressSpace(startupData->m_addressSpace);
    OriginTrialContext::addTokens(context, startupData->m_originTrialTokens.get());

    return context;
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(const KURL& url, const String& userAgent, ServiceWorkerThread* thread, double timeOrigin, std::unique_ptr<SecurityOrigin::PrivilegeData> starterOriginPrivilegeData, WorkerClients* workerClients)
    : WorkerGlobalScope(url, userAgent, thread, timeOrigin, std::move(starterOriginPrivilegeData), workerClients)
    , m_didEvaluateScript(false)
    , m_hadErrorInTopLevelEventHandler(false)
    , m_eventNestingLevel(0)
    , m_scriptCount(0)
    , m_scriptTotalSize(0)
    , m_scriptCachedMetadataTotalSize(0)
{
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope()
{
}

void ServiceWorkerGlobalScope::didEvaluateWorkerScript()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scriptCountHistogram, new CustomCountHistogram("ServiceWorker.ScriptCount", 1, 1000, 50));
    scriptCountHistogram.count(m_scriptCount);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scriptTotalSizeHistogram, new CustomCountHistogram("ServiceWorker.ScriptTotalSize", 1000, 5000000, 50));
    scriptTotalSizeHistogram.count(m_scriptTotalSize);
    if (m_scriptCachedMetadataTotalSize) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, cachedMetadataHistogram, new CustomCountHistogram("ServiceWorker.ScriptCachedMetadataTotalSize", 1000, 50000000, 50));
        cachedMetadataHistogram.count(m_scriptCachedMetadataTotalSize);
    }
    m_didEvaluateScript = true;
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* scriptState, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    return GlobalFetch::fetch(scriptState, *this, input, init, exceptionState);
}

ServiceWorkerClients* ServiceWorkerGlobalScope::clients()
{
    if (!m_clients)
        m_clients = ServiceWorkerClients::create();
    return m_clients;
}

ServiceWorkerRegistration* ServiceWorkerGlobalScope::registration()
{
    return m_registration;
}

ScriptPromise ServiceWorkerGlobalScope::skipWaiting(ScriptState* scriptState)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    // FIXME: short-term fix, see details at: https://codereview.chromium.org/535193002/.
    if (!executionContext)
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    ServiceWorkerGlobalScopeClient::from(executionContext)->skipWaiting(new CallbackPromiseAdapter<void, void>(resolver));
    return promise;
}

void ServiceWorkerGlobalScope::setRegistration(std::unique_ptr<WebServiceWorkerRegistration::Handle> handle)
{
    if (!getExecutionContext())
        return;
    m_registration = ServiceWorkerRegistration::getOrCreate(getExecutionContext(), wrapUnique(handle.release()));
}

bool ServiceWorkerGlobalScope::addEventListenerInternal(const AtomicString& eventType, EventListener* listener, const AddEventListenerOptions& options)
{
    if (m_didEvaluateScript) {
        if (eventType == EventTypeNames::install) {
            ConsoleMessage* consoleMessage = ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Event handler of 'install' event must be added on the initial evaluation of worker script.");
            addMessageToWorkerConsole(consoleMessage);
        } else if (eventType == EventTypeNames::activate) {
            ConsoleMessage* consoleMessage = ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Event handler of 'activate' event must be added on the initial evaluation of worker script.");
            addMessageToWorkerConsole(consoleMessage);
        }
    }
    return WorkerGlobalScope::addEventListenerInternal(eventType, listener, options);
}

const AtomicString& ServiceWorkerGlobalScope::interfaceName() const
{
    return EventTargetNames::ServiceWorkerGlobalScope;
}

DispatchEventResult ServiceWorkerGlobalScope::dispatchEventInternal(Event* event)
{
    m_eventNestingLevel++;
    DispatchEventResult dispatchResult = WorkerGlobalScope::dispatchEventInternal(event);
    if (event->interfaceName() == EventNames::ErrorEvent && m_eventNestingLevel == 2)
        m_hadErrorInTopLevelEventHandler = true;
    m_eventNestingLevel--;
    return dispatchResult;
}

void ServiceWorkerGlobalScope::dispatchExtendableEvent(Event* event, WaitUntilObserver* observer)
{
    ASSERT(m_eventNestingLevel == 0);
    m_hadErrorInTopLevelEventHandler = false;

    observer->willDispatchEvent();
    dispatchEvent(event);
    if (thread()->terminated())
        m_hadErrorInTopLevelEventHandler = true;
    observer->didDispatchEvent(m_hadErrorInTopLevelEventHandler);
}

DEFINE_TRACE(ServiceWorkerGlobalScope)
{
    visitor->trace(m_clients);
    visitor->trace(m_registration);
    WorkerGlobalScope::trace(visitor);
}

void ServiceWorkerGlobalScope::importScripts(const Vector<String>& urls, ExceptionState& exceptionState)
{
    // Bust the MemoryCache to ensure script requests reach the browser-side
    // and get added to and retrieved from the ServiceWorker's script cache.
    // FIXME: Revisit in light of the solution to crbug/388375.
    for (Vector<String>::const_iterator it = urls.begin(); it != urls.end(); ++it)
        getExecutionContext()->removeURLFromMemoryCache(completeURL(*it));
    WorkerGlobalScope::importScripts(urls, exceptionState);
}

CachedMetadataHandler* ServiceWorkerGlobalScope::createWorkerScriptCachedMetadataHandler(const KURL& scriptURL, const Vector<char>* metaData)
{
    return ServiceWorkerScriptCachedMetadataHandler::create(this, scriptURL, metaData);
}

void ServiceWorkerGlobalScope::logExceptionToConsole(const String& errorMessage, std::unique_ptr<SourceLocation> location)
{
    WorkerGlobalScope::logExceptionToConsole(errorMessage, location->clone());
    ConsoleMessage* consoleMessage = ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, errorMessage, std::move(location));
    addMessageToWorkerConsole(consoleMessage);
}

void ServiceWorkerGlobalScope::scriptLoaded(size_t scriptSize, size_t cachedMetadataSize)
{
    ++m_scriptCount;
    m_scriptTotalSize += scriptSize;
    m_scriptCachedMetadataTotalSize += cachedMetadataSize;
}

} // namespace blink
