/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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
#include "core/dom/ExecutionContext.h"

#include "core/dom/AddConsoleMessageTask.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/EventTarget.h"
#include "core/html/PublicURLManager.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "wtf/MainThread.h"

namespace WebCore {

class ExecutionContext::PendingException : public NoBaseWillBeGarbageCollectedFinalized<ExecutionContext::PendingException> {
    WTF_MAKE_NONCOPYABLE(PendingException);
public:
    PendingException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
        : m_errorMessage(errorMessage)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_sourceURL(sourceURL)
        , m_callStack(callStack)
    {
    }
    void trace(Visitor* visitor)
    {
        visitor->trace(m_callStack);
    }
    String m_errorMessage;
    int m_lineNumber;
    int m_columnNumber;
    String m_sourceURL;
    RefPtrWillBeMember<ScriptCallStack> m_callStack;
};

ExecutionContext::ExecutionContext()
    : m_client(0)
    , m_sandboxFlags(SandboxNone)
    , m_circularSequentialID(0)
    , m_inDispatchErrorEvent(false)
    , m_activeDOMObjectsAreSuspended(false)
    , m_activeDOMObjectsAreStopped(false)
{
}

ExecutionContext::~ExecutionContext()
{
}

bool ExecutionContext::hasPendingActivity()
{
    return lifecycleNotifier().hasPendingActivity();
}

void ExecutionContext::suspendActiveDOMObjects()
{
    lifecycleNotifier().notifySuspendingActiveDOMObjects();
    m_activeDOMObjectsAreSuspended = true;
}

void ExecutionContext::resumeActiveDOMObjects()
{
    m_activeDOMObjectsAreSuspended = false;
    lifecycleNotifier().notifyResumingActiveDOMObjects();
}

void ExecutionContext::stopActiveDOMObjects()
{
    m_activeDOMObjectsAreStopped = true;
    lifecycleNotifier().notifyStoppingActiveDOMObjects();
}

unsigned ExecutionContext::activeDOMObjectCount()
{
    return lifecycleNotifier().activeDOMObjects().size();
}

void ExecutionContext::suspendScheduledTasks()
{
    suspendActiveDOMObjects();
    if (m_client)
        m_client->tasksWereSuspended();
}

void ExecutionContext::resumeScheduledTasks()
{
    resumeActiveDOMObjects();
    if (m_client)
        m_client->tasksWereResumed();
}

void ExecutionContext::suspendActiveDOMObjectIfNeeded(ActiveDOMObject* object)
{
    ASSERT(lifecycleNotifier().contains(object));
    // Ensure all ActiveDOMObjects are suspended also newly created ones.
    if (m_activeDOMObjectsAreSuspended)
        object->suspend();
}

bool ExecutionContext::shouldSanitizeScriptError(const String& sourceURL, AccessControlStatus corsStatus)
{
    return !(securityOrigin()->canRequest(completeURL(sourceURL)) || corsStatus == SharableCrossOrigin);
}

void ExecutionContext::reportException(PassRefPtrWillBeRawPtr<ErrorEvent> event, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, AccessControlStatus corsStatus)
{
    RefPtrWillBeRawPtr<ErrorEvent> errorEvent = event;
    if (m_inDispatchErrorEvent) {
        if (!m_pendingExceptions)
            m_pendingExceptions = adoptPtrWillBeNoop(new WillBeHeapVector<OwnPtrWillBeMember<PendingException> >());
        m_pendingExceptions->append(adoptPtrWillBeNoop(new PendingException(errorEvent->messageForConsole(), errorEvent->lineno(), errorEvent->colno(), errorEvent->filename(), callStack)));
        return;
    }

    // First report the original exception and only then all the nested ones.
    if (!dispatchErrorEvent(errorEvent, corsStatus) && m_client)
        m_client->logExceptionToConsole(errorEvent->messageForConsole(), errorEvent->filename(), errorEvent->lineno(), errorEvent->colno(), callStack);

    if (!m_pendingExceptions)
        return;

    for (size_t i = 0; i < m_pendingExceptions->size(); i++) {
        PendingException* e = m_pendingExceptions->at(i).get();
        if (m_client)
            m_client->logExceptionToConsole(e->m_errorMessage, e->m_sourceURL, e->m_lineNumber, e->m_columnNumber, e->m_callStack);
    }
    m_pendingExceptions.clear();
}

void ExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber)
{
    if (!m_client)
        return;
    m_client->addMessage(source, level, message, sourceURL, lineNumber, 0);
}

void ExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, ScriptState* scriptState)
{
    if (!m_client)
        return;
    m_client->addMessage(source, level, message, String(), 0, scriptState);
}

bool ExecutionContext::dispatchErrorEvent(PassRefPtrWillBeRawPtr<ErrorEvent> event, AccessControlStatus corsStatus)
{
    if (!m_client)
        return false;
    EventTarget* target = m_client->errorEventTarget();
    if (!target)
        return false;

    RefPtrWillBeRawPtr<ErrorEvent> errorEvent = event;
    if (shouldSanitizeScriptError(errorEvent->filename(), corsStatus))
        errorEvent = ErrorEvent::createSanitizedError(errorEvent->world());

    ASSERT(!m_inDispatchErrorEvent);
    m_inDispatchErrorEvent = true;
    target->dispatchEvent(errorEvent);
    m_inDispatchErrorEvent = false;
    return errorEvent->defaultPrevented();
}

int ExecutionContext::circularSequentialID()
{
    ++m_circularSequentialID;
    if (m_circularSequentialID <= 0)
        m_circularSequentialID = 1;
    return m_circularSequentialID;
}

int ExecutionContext::installNewTimeout(PassOwnPtr<ScheduledAction> action, int timeout, bool singleShot)
{
    int timeoutID;
    while (true) {
        timeoutID = circularSequentialID();
        if (!m_timeouts.contains(timeoutID))
            break;
    }
    TimeoutMap::AddResult result = m_timeouts.add(timeoutID, DOMTimer::create(this, action, timeout, singleShot, timeoutID));
    ASSERT(result.isNewEntry);
    DOMTimer* timer = result.storedValue->value.get();

    timer->suspendIfNeeded();

    return timer->timeoutID();
}

void ExecutionContext::removeTimeoutByID(int timeoutID)
{
    if (timeoutID <= 0)
        return;
    m_timeouts.remove(timeoutID);
}

PublicURLManager& ExecutionContext::publicURLManager()
{
    if (!m_publicURLManager)
        m_publicURLManager = PublicURLManager::create(this);
    return *m_publicURLManager;
}

void ExecutionContext::didChangeTimerAlignmentInterval()
{
    for (TimeoutMap::iterator iter = m_timeouts.begin(); iter != m_timeouts.end(); ++iter)
        iter->value->didChangeAlignmentInterval();
}

SecurityOrigin* ExecutionContext::securityOrigin() const
{
    RELEASE_ASSERT(m_client);
    return m_client->securityContext().securityOrigin();
}

ContentSecurityPolicy* ExecutionContext::contentSecurityPolicy() const
{
    RELEASE_ASSERT(m_client);
    return m_client->securityContext().contentSecurityPolicy();
}

const KURL& ExecutionContext::url() const
{
    if (!m_client) {
        DEFINE_STATIC_LOCAL(KURL, emptyURL, ());
        return emptyURL;
    }

    return virtualURL();
}

KURL ExecutionContext::completeURL(const String& url) const
{

    if (!m_client) {
        DEFINE_STATIC_LOCAL(KURL, emptyURL, ());
        return emptyURL;
    }

    return virtualCompleteURL(url);
}

void ExecutionContext::disableEval(const String& errorMessage)
{
    if (!m_client)
        return;
    return m_client->disableEval(errorMessage);
}

LocalDOMWindow* ExecutionContext::executingWindow() const
{
    RELEASE_ASSERT(m_client);
    return m_client->executingWindow();
}

String ExecutionContext::userAgent(const KURL& url) const
{
    if (!m_client)
        return String();
    return m_client->userAgent(url);
}

double ExecutionContext::timerAlignmentInterval() const
{
    if (!m_client)
        return DOMTimer::visiblePageAlignmentInterval();
    return m_client->timerAlignmentInterval();
}

void ExecutionContext::postTask(PassOwnPtr<ExecutionContextTask> task)
{
    if (!m_client)
        return;
    m_client->postTask(task);
}

void ExecutionContext::postTask(const Closure& closure)
{
    if (!m_client)
        return;
    m_client->postTask(CallClosureTask::create(closure));
}

PassOwnPtr<LifecycleNotifier<ExecutionContext> > ExecutionContext::createLifecycleNotifier()
{
    return ContextLifecycleNotifier::create(this);
}

ContextLifecycleNotifier& ExecutionContext::lifecycleNotifier()
{
    return static_cast<ContextLifecycleNotifier&>(LifecycleContext<ExecutionContext>::lifecycleNotifier());
}

bool ExecutionContext::isIteratingOverObservers() const
{
    return m_lifecycleNotifier && m_lifecycleNotifier->isIteratingOverObservers();
}

void ExecutionContext::enforceSandboxFlags(SandboxFlags mask)
{
    m_sandboxFlags |= mask;

    RELEASE_ASSERT(m_client);
    // The SandboxOrigin is stored redundantly in the security origin.
    if (isSandboxed(SandboxOrigin) && m_client->securityContext().securityOrigin() && !m_client->securityContext().securityOrigin()->isUnique()) {
        m_client->securityContext().setSecurityOrigin(SecurityOrigin::createUnique());
        m_client->didUpdateSecurityOrigin();
    }
}

void ExecutionContext::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_pendingExceptions);
#endif
    Supplementable<WebCore::ExecutionContext>::trace(visitor);
}

} // namespace WebCore
