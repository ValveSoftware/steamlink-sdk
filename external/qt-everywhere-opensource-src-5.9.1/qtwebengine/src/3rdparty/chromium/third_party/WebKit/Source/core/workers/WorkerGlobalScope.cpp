/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/workers/WorkerGlobalScope.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/ErrorEvent.h"
#include "core/events/Event.h"
#include "core/fetch/MemoryCache.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerLocation.h"
#include "core/workers/WorkerNavigator.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "platform/InstanceCounters.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

void removeURLFromMemoryCacheInternal(const KURL& url) {
  memoryCache()->removeURLFromCache(url);
}

}  // namespace

WorkerGlobalScope::~WorkerGlobalScope() {
  DCHECK(!m_scriptController);
  InstanceCounters::decrementCounter(
      InstanceCounters::WorkerGlobalScopeCounter);
}

void WorkerGlobalScope::countFeature(UseCounter::Feature) const {
  // TODO(nhiroki): How should we count features for shared/service workers?
  // (http://crbug.com/376039)
}

void WorkerGlobalScope::countDeprecation(UseCounter::Feature feature) const {
  // TODO(nhiroki): How should we count features for shared/service workers?
  // (http://crbug.com/376039)

  DCHECK(isSharedWorkerGlobalScope() || isServiceWorkerGlobalScope() ||
         isCompositorWorkerGlobalScope());
  DCHECK(feature != UseCounter::OBSOLETE_PageDestruction);
  DCHECK(feature < UseCounter::NumberOfFeatures);

  // For each deprecated feature, send console message at most once
  // per worker lifecycle.
  if (!m_deprecationWarningBits.quickGet(feature)) {
    m_deprecationWarningBits.quickSet(feature);
    DCHECK(!Deprecation::deprecationMessage(feature).isEmpty());
    DCHECK(getExecutionContext());
    getExecutionContext()->addConsoleMessage(
        ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel,
                               Deprecation::deprecationMessage(feature)));
  }
}

KURL WorkerGlobalScope::completeURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // FIXME: Should we change the KURL constructor to have this behavior?
  if (url.isNull())
    return KURL();
  // Always use UTF-8 in Workers.
  return KURL(m_url, url);
}

void WorkerGlobalScope::dispose() {
  DCHECK(thread()->isCurrentThread());

  // Event listeners would keep DOMWrapperWorld objects alive for too long.
  // Also, they have references to JS objects, which become dangling once Heap
  // is destroyed.
  for (auto it = m_eventListeners.begin(); it != m_eventListeners.end();) {
    V8AbstractEventListener* listener = *it;
    // clearListenerObject() will unregister the listener from
    // m_eventListeners, and invalidate the iterator, so we have to advance
    // it first.
    ++it;
    listener->clearListenerObject();
  }
  removeAllEventListeners();

  m_scriptController->dispose();
  m_scriptController.clear();
  m_eventQueue->close();
  m_thread = nullptr;
}

void WorkerGlobalScope::exceptionUnhandled(int exceptionId) {
  ErrorEvent* event = m_pendingErrorEvents.take(exceptionId);
  DCHECK(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::from(thread()->isolate()))
    debugger->exceptionThrown(m_thread, event);
}

void WorkerGlobalScope::registerEventListener(
    V8AbstractEventListener* eventListener) {
  bool newEntry = m_eventListeners.add(eventListener).isNewEntry;
  RELEASE_ASSERT(newEntry);
}

void WorkerGlobalScope::deregisterEventListener(
    V8AbstractEventListener* eventListener) {
  auto it = m_eventListeners.find(eventListener);
  RELEASE_ASSERT(it != m_eventListeners.end());
  m_eventListeners.remove(it);
}

WorkerLocation* WorkerGlobalScope::location() const {
  if (!m_location)
    m_location = WorkerLocation::create(m_url);
  return m_location.get();
}

WorkerNavigator* WorkerGlobalScope::navigator() const {
  if (!m_navigator)
    m_navigator = WorkerNavigator::create(m_userAgent);
  return m_navigator.get();
}

void WorkerGlobalScope::close() {
  // Let current script run to completion, but tell the worker micro task
  // runner to tear down the thread after this task.
  m_closing = true;
}

void WorkerGlobalScope::importScripts(const Vector<String>& urls,
                                      ExceptionState& exceptionState) {
  DCHECK(contentSecurityPolicy());
  DCHECK(getExecutionContext());

  ExecutionContext& executionContext = *this->getExecutionContext();

  Vector<KURL> completedURLs;
  for (const String& urlString : urls) {
    const KURL& url = executionContext.completeURL(urlString);
    if (!url.isValid()) {
      exceptionState.throwDOMException(
          SyntaxError, "The URL '" + urlString + "' is invalid.");
      return;
    }
    if (!contentSecurityPolicy()->allowScriptFromSource(url, AtomicString(),
                                                        NotParserInserted)) {
      exceptionState.throwDOMException(
          NetworkError,
          "The script at '" + url.elidedString() + "' failed to load.");
      return;
    }
    completedURLs.append(url);
  }

  for (const KURL& completeURL : completedURLs) {
    RefPtr<WorkerScriptLoader> scriptLoader(WorkerScriptLoader::create());
    scriptLoader->setRequestContext(WebURLRequest::RequestContextScript);
    scriptLoader->loadSynchronously(
        executionContext, completeURL, AllowCrossOriginRequests,
        executionContext.securityContext().addressSpace());

    // If the fetching attempt failed, throw a NetworkError exception and
    // abort all these steps.
    if (scriptLoader->failed()) {
      exceptionState.throwDOMException(
          NetworkError,
          "The script at '" + completeURL.elidedString() + "' failed to load.");
      return;
    }

    InspectorInstrumentation::scriptImported(
        &executionContext, scriptLoader->identifier(), scriptLoader->script());

    ErrorEvent* errorEvent = nullptr;
    std::unique_ptr<Vector<char>> cachedMetaData(
        scriptLoader->releaseCachedMetadata());
    CachedMetadataHandler* handler(createWorkerScriptCachedMetadataHandler(
        completeURL, cachedMetaData.get()));
    thread()->workerReportingProxy().willEvaluateImportedScript(
        scriptLoader->script().length(),
        scriptLoader->cachedMetadata() ? scriptLoader->cachedMetadata()->size()
                                       : 0);
    m_scriptController->evaluate(
        ScriptSourceCode(scriptLoader->script(), scriptLoader->responseURL()),
        &errorEvent, handler, m_v8CacheOptions);
    if (errorEvent) {
      m_scriptController->rethrowExceptionFromImportedScript(errorEvent,
                                                             exceptionState);
      return;
    }
  }
}

v8::Local<v8::Object> WorkerGlobalScope::wrap(
    v8::Isolate*,
    v8::Local<v8::Object> creationContext) {
  // WorkerGlobalScope must never be wrapped with wrap method.  The global
  // object of ECMAScript environment is used as the wrapper.
  RELEASE_NOTREACHED();
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkerGlobalScope::associateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  RELEASE_NOTREACHED();  // same as wrap method
  return v8::Local<v8::Object>();
}

bool WorkerGlobalScope::hasPendingActivity() const {
  return m_timers.hasInstalledTimeout();
}

bool WorkerGlobalScope::isJSExecutionForbidden() const {
  return m_scriptController->isExecutionForbidden();
}

bool WorkerGlobalScope::isContextThread() const {
  return thread()->isCurrentThread();
}

void WorkerGlobalScope::disableEval(const String& errorMessage) {
  m_scriptController->disableEval(errorMessage);
}

void WorkerGlobalScope::postTask(const WebTraceLocation& location,
                                 std::unique_ptr<ExecutionContextTask> task,
                                 const String& taskNameForInstrumentation) {
  thread()->postTask(location, std::move(task),
                     !taskNameForInstrumentation.isEmpty());
}

void WorkerGlobalScope::addConsoleMessage(ConsoleMessage* consoleMessage) {
  DCHECK(isContextThread());
  thread()->workerReportingProxy().reportConsoleMessage(
      consoleMessage->source(), consoleMessage->level(),
      consoleMessage->message(), consoleMessage->location());
  thread()->consoleMessageStorage()->addConsoleMessage(this, consoleMessage);
}

WorkerEventQueue* WorkerGlobalScope::getEventQueue() const {
  return m_eventQueue.get();
}

bool WorkerGlobalScope::isSecureContext(
    String& errorMessage,
    const SecureContextCheck privilegeContextCheck) const {
  // Until there are APIs that are available in workers and that
  // require a privileged context test that checks ancestors, just do
  // a simple check here. Once we have a need for a real
  // |isSecureContext| check here, we can check the responsible
  // document for a privileged context at worker creation time, pass
  // it in via WorkerThreadStartupData, and check it here.
  if (getSecurityOrigin()->isPotentiallyTrustworthy())
    return true;
  errorMessage = getSecurityOrigin()->isPotentiallyTrustworthyErrorMessage();
  return false;
}

ExecutionContext* WorkerGlobalScope::getExecutionContext() const {
  return const_cast<WorkerGlobalScope*>(this);
}

WorkerGlobalScope::WorkerGlobalScope(
    const KURL& url,
    const String& userAgent,
    WorkerThread* thread,
    double timeOrigin,
    std::unique_ptr<SecurityOrigin::PrivilegeData> starterOriginPrivilageData,
    WorkerClients* workerClients)
    : ActiveScriptWrappable(this),
      m_url(url),
      m_userAgent(userAgent),
      m_v8CacheOptions(V8CacheOptionsDefault),
      m_deprecationWarningBits(UseCounter::NumberOfFeatures),
      m_scriptController(
          WorkerOrWorkletScriptController::create(this, thread->isolate())),
      m_thread(thread),
      m_closing(false),
      m_eventQueue(WorkerEventQueue::create(this)),
      m_workerClients(workerClients),
      m_timers(Platform::current()
                   ->currentThread()
                   ->scheduler()
                   ->timerTaskRunner()
                   ->clone()),
      m_timeOrigin(timeOrigin),
      m_lastPendingErrorEventId(0) {
  InstanceCounters::incrementCounter(
      InstanceCounters::WorkerGlobalScopeCounter);
  setSecurityOrigin(SecurityOrigin::create(url));
  if (starterOriginPrivilageData)
    getSecurityOrigin()->transferPrivilegesFrom(
        std::move(starterOriginPrivilageData));

  if (m_workerClients)
    m_workerClients->reattachThread();
}

void WorkerGlobalScope::applyContentSecurityPolicyFromVector(
    const Vector<CSPHeaderAndType>& headers) {
  if (!contentSecurityPolicy()) {
    ContentSecurityPolicy* csp = ContentSecurityPolicy::create();
    setContentSecurityPolicy(csp);
  }
  for (const auto& policyAndType : headers)
    contentSecurityPolicy()->didReceiveHeader(
        policyAndType.first, policyAndType.second,
        ContentSecurityPolicyHeaderSourceHTTP);
  contentSecurityPolicy()->bindToExecutionContext(getExecutionContext());
}

void WorkerGlobalScope::setWorkerSettings(
    std::unique_ptr<WorkerSettings> workerSettings) {
  m_workerSettings = std::move(workerSettings);
}

void WorkerGlobalScope::exceptionThrown(ErrorEvent* event) {
  int nextId = ++m_lastPendingErrorEventId;
  m_pendingErrorEvents.set(nextId, event);
  thread()->workerReportingProxy().reportException(
      event->messageForConsole(), event->location()->clone(), nextId);
}

void WorkerGlobalScope::removeURLFromMemoryCache(const KURL& url) {
  m_thread->workerLoaderProxy()->postTaskToLoader(
      BLINK_FROM_HERE,
      createCrossThreadTask(&removeURLFromMemoryCacheInternal, url));
}

KURL WorkerGlobalScope::virtualCompleteURL(const String& url) const {
  return completeURL(url);
}

DEFINE_TRACE(WorkerGlobalScope) {
  visitor->trace(m_location);
  visitor->trace(m_navigator);
  visitor->trace(m_scriptController);
  visitor->trace(m_eventQueue);
  visitor->trace(m_timers);
  visitor->trace(m_eventListeners);
  visitor->trace(m_pendingErrorEvents);
  ExecutionContext::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
  SecurityContext::trace(visitor);
  Supplementable<WorkerGlobalScope>::trace(visitor);
}

}  // namespace blink
