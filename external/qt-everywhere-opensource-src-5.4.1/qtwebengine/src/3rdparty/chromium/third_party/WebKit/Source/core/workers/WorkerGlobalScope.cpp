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

#include "config.h"
#include "core/workers/WorkerGlobalScope.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScheduledAction.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/AddConsoleMessageTask.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/DOMURL.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/events/ErrorEvent.h"
#include "core/events/Event.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/workers/WorkerNavigator.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerConsole.h"
#include "core/workers/WorkerLocation.h"
#include "core/workers/WorkerNavigator.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

class CloseWorkerGlobalScopeTask : public ExecutionContextTask {
public:
    static PassOwnPtr<CloseWorkerGlobalScopeTask> create()
    {
        return adoptPtr(new CloseWorkerGlobalScopeTask);
    }

    virtual void performTask(ExecutionContext *context)
    {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        // Notify parent that this context is closed. Parent is responsible for calling WorkerThread::stop().
        workerGlobalScope->thread()->workerReportingProxy().workerGlobalScopeClosed();
    }

    virtual bool isCleanupTask() const { return true; }
};

WorkerGlobalScope::WorkerGlobalScope(const KURL& url, const String& userAgent, WorkerThread* thread, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerClients> workerClients)
    : m_url(url)
    , m_userAgent(userAgent)
    , m_script(adoptPtr(new WorkerScriptController(*this)))
    , m_thread(thread)
    , m_workerInspectorController(adoptPtr(new WorkerInspectorController(this)))
    , m_closing(false)
    , m_eventQueue(WorkerEventQueue::create(this))
    , m_workerClients(workerClients)
    , m_timeOrigin(timeOrigin)
    , m_terminationObserver(0)
{
    ScriptWrappable::init(this);
    setClient(this);
    setSecurityOrigin(SecurityOrigin::create(url));
    m_workerClients->reattachThread();
}

WorkerGlobalScope::~WorkerGlobalScope()
{
}

void WorkerGlobalScope::applyContentSecurityPolicyFromString(const String& policy, ContentSecurityPolicyHeaderType contentSecurityPolicyType)
{
    setContentSecurityPolicy(ContentSecurityPolicy::create(this));
    contentSecurityPolicy()->didReceiveHeader(policy, contentSecurityPolicyType, ContentSecurityPolicyHeaderSourceHTTP);
}

ExecutionContext* WorkerGlobalScope::executionContext() const
{
    return const_cast<WorkerGlobalScope*>(this);
}

const KURL& WorkerGlobalScope::virtualURL() const
{
    return m_url;
}

KURL WorkerGlobalScope::virtualCompleteURL(const String& url) const
{
    return completeURL(url);
}

KURL WorkerGlobalScope::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    if (url.isNull())
        return KURL();
    // Always use UTF-8 in Workers.
    return KURL(m_url, url);
}

String WorkerGlobalScope::userAgent(const KURL&) const
{
    return m_userAgent;
}

void WorkerGlobalScope::disableEval(const String& errorMessage)
{
    m_script->disableEval(errorMessage);
}

double WorkerGlobalScope::timerAlignmentInterval() const
{
    return DOMTimer::visiblePageAlignmentInterval();
}

WorkerLocation* WorkerGlobalScope::location() const
{
    if (!m_location)
        m_location = WorkerLocation::create(m_url);
    return m_location.get();
}

void WorkerGlobalScope::close()
{
    if (m_closing)
        return;

    // Let current script run to completion but prevent future script evaluations.
    // After m_closing is set, all the tasks in the queue continue to be fetched but only
    // tasks with isCleanupTask()==true will be executed.
    m_closing = true;
    postTask(CloseWorkerGlobalScopeTask::create());
}

WorkerConsole* WorkerGlobalScope::console()
{
    if (!m_console)
        m_console = WorkerConsole::create(this);
    return m_console.get();
}

WorkerNavigator* WorkerGlobalScope::navigator() const
{
    if (!m_navigator)
        m_navigator = WorkerNavigator::create(m_userAgent);
    return m_navigator.get();
}

void WorkerGlobalScope::postTask(PassOwnPtr<ExecutionContextTask> task)
{
    thread()->runLoop().postTask(task);
}

void WorkerGlobalScope::clearInspector()
{
    m_workerInspectorController.clear();
}

void WorkerGlobalScope::registerTerminationObserver(TerminationObserver* observer)
{
    ASSERT(!m_terminationObserver);
    ASSERT(observer);
    m_terminationObserver = observer;
}

void WorkerGlobalScope::unregisterTerminationObserver(TerminationObserver* observer)
{
    ASSERT(observer);
    ASSERT(m_terminationObserver == observer);
    m_terminationObserver = 0;
}

void WorkerGlobalScope::wasRequestedToTerminate()
{
    if (m_terminationObserver)
        m_terminationObserver->wasRequestedToTerminate();
}

void WorkerGlobalScope::dispose()
{
    ASSERT(thread()->isCurrentThread());

    m_eventQueue->close();
    clearScript();
    clearInspector();
    setClient(0);

    // We do not clear the thread field of the
    // WorkerGlobalScope. Other objects keep the worker global scope
    // alive because they need its thread field to check that work is
    // being carried out on the right thread. We therefore cannot clear
    // the thread field before all references to the worker global
    // scope are gone.
}

void WorkerGlobalScope::importScripts(const Vector<String>& urls, ExceptionState& exceptionState)
{
    ASSERT(contentSecurityPolicy());
    ASSERT(executionContext());

    ExecutionContext& executionContext = *this->executionContext();

    Vector<String>::const_iterator urlsEnd = urls.end();
    Vector<KURL> completedURLs;
    for (Vector<String>::const_iterator it = urls.begin(); it != urlsEnd; ++it) {
        const KURL& url = executionContext.completeURL(*it);
        if (!url.isValid()) {
            exceptionState.throwDOMException(SyntaxError, "The URL '" + *it + "' is invalid.");
            return;
        }
        if (!contentSecurityPolicy()->allowScriptFromSource(url)) {
            exceptionState.throwDOMException(NetworkError, "The script at '" + url.elidedString() + "' failed to load.");
            return;
        }
        completedURLs.append(url);
    }
    Vector<KURL>::const_iterator end = completedURLs.end();

    for (Vector<KURL>::const_iterator it = completedURLs.begin(); it != end; ++it) {
        RefPtr<WorkerScriptLoader> scriptLoader(WorkerScriptLoader::create());
        scriptLoader->setTargetType(ResourceRequest::TargetIsScript);
        scriptLoader->loadSynchronously(executionContext, *it, AllowCrossOriginRequests);

        // If the fetching attempt failed, throw a NetworkError exception and abort all these steps.
        if (scriptLoader->failed()) {
            exceptionState.throwDOMException(NetworkError, "The script at '" + it->elidedString() + "' failed to load.");
            return;
        }

        InspectorInstrumentation::scriptImported(&executionContext, scriptLoader->identifier(), scriptLoader->script());

        RefPtrWillBeRawPtr<ErrorEvent> errorEvent = nullptr;
        m_script->evaluate(ScriptSourceCode(scriptLoader->script(), scriptLoader->responseURL()), &errorEvent);
        if (errorEvent) {
            m_script->rethrowExceptionFromImportedScript(errorEvent.release());
            return;
        }
    }
}

EventTarget* WorkerGlobalScope::errorEventTarget()
{
    return this;
}

void WorkerGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>)
{
    thread()->workerReportingProxy().reportException(errorMessage, lineNumber, columnNumber, sourceURL);
}

void WorkerGlobalScope::reportBlockedScriptExecutionToInspector(const String& directiveText)
{
    InspectorInstrumentation::scriptExecutionBlockedByCSP(this, directiveText);
}

void WorkerGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState* scriptState)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask::create(source, level, message));
        return;
    }
    thread()->workerReportingProxy().reportConsoleMessage(source, level, message, lineNumber, sourceURL);
    addMessageToWorkerConsole(source, level, message, sourceURL, lineNumber, nullptr, scriptState);
}

void WorkerGlobalScope::addMessageToWorkerConsole(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, ScriptState* scriptState)
{
    ASSERT(isContextThread());
    if (callStack)
        InspectorInstrumentation::addMessageToConsole(this, source, LogMessageType, level, message, callStack);
    else
        InspectorInstrumentation::addMessageToConsole(this, source, LogMessageType, level, message, sourceURL, lineNumber, 0, scriptState);
}

bool WorkerGlobalScope::isContextThread() const
{
    return thread()->isCurrentThread();
}

bool WorkerGlobalScope::isJSExecutionForbidden() const
{
    return m_script->isExecutionForbidden();
}

bool WorkerGlobalScope::idleNotification()
{
    return script()->idleNotification();
}

WorkerEventQueue* WorkerGlobalScope::eventQueue() const
{
    return m_eventQueue.get();
}

void WorkerGlobalScope::countFeature(UseCounter::Feature) const
{
    // FIXME: How should we count features for shared/service workers?
}

void WorkerGlobalScope::countDeprecation(UseCounter::Feature) const
{
    // FIXME: How should we count features for shared/service workers?
}

void WorkerGlobalScope::trace(Visitor* visitor)
{
    visitor->trace(m_console);
    visitor->trace(m_location);
    visitor->trace(m_navigator);
    visitor->trace(m_eventQueue);
    visitor->trace(m_workerClients);
    WillBeHeapSupplementable<WorkerGlobalScope>::trace(visitor);
    ExecutionContext::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
