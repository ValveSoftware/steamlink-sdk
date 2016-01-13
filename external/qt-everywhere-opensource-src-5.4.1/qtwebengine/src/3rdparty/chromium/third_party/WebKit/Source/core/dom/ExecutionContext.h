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

#ifndef ExecutionContext_h
#define ExecutionContext_h

#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContextClient.h"
#include "core/dom/SandboxFlags.h"
#include "core/dom/SecurityContext.h"
#include "core/events/ErrorEvent.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/frame/ConsoleTypes.h"
#include "core/frame/DOMTimer.h"
#include "platform/LifecycleContext.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Functional.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WTF {
class OrdinalNumber;
}

namespace WebCore {

class ContextLifecycleNotifier;
class LocalDOMWindow;
class EventListener;
class EventQueue;
class EventTarget;
class ExecutionContextTask;
class ScriptState;
class PublicURLManager;
class SecurityOrigin;
class ScriptCallStack;

class ExecutionContext
    : public WillBeGarbageCollectedMixin
    , public LifecycleContext<ExecutionContext>
    , public Supplementable<ExecutionContext> {
public:
    virtual void trace(Visitor*);

    // Delegating to ExecutionContextClient
    bool isDocument() const { return m_client && m_client->isDocument(); }
    bool isWorkerGlobalScope() const { return m_client && m_client->isWorkerGlobalScope(); }
    bool isJSExecutionForbidden() { return m_client && m_client->isJSExecutionForbidden(); }
    SecurityOrigin* securityOrigin() const;
    ContentSecurityPolicy* contentSecurityPolicy() const;
    const KURL& url() const;
    KURL completeURL(const String& url) const;
    void disableEval(const String& errorMessage);
    LocalDOMWindow* executingWindow() const;
    String userAgent(const KURL&) const;
    void postTask(PassOwnPtr<ExecutionContextTask>);
    void postTask(const Closure&);
    double timerAlignmentInterval() const;

    virtual void reportBlockedScriptExecutionToInspector(const String& directiveText) = 0;

    virtual SecurityContext& securityContext() = 0;
    KURL contextURL() const { return virtualURL(); }
    KURL contextCompleteURL(const String& url) const { return virtualCompleteURL(url); }

    bool shouldSanitizeScriptError(const String& sourceURL, AccessControlStatus);
    void reportException(PassRefPtrWillBeRawPtr<ErrorEvent>, PassRefPtrWillBeRawPtr<ScriptCallStack>, AccessControlStatus);

    void addConsoleMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber);
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, ScriptState* = 0);

    PublicURLManager& publicURLManager();

    // Active objects are not garbage collected even if inaccessible, e.g. because their activity may result in callbacks being invoked.
    bool hasPendingActivity();

    void suspendActiveDOMObjects();
    void resumeActiveDOMObjects();
    void stopActiveDOMObjects();
    unsigned activeDOMObjectCount();

    virtual void suspendScheduledTasks();
    virtual void resumeScheduledTasks();
    virtual bool tasksNeedSuspension() { return false; }

    bool activeDOMObjectsAreSuspended() const { return m_activeDOMObjectsAreSuspended; }
    bool activeDOMObjectsAreStopped() const { return m_activeDOMObjectsAreStopped; }
    bool isIteratingOverObservers() const;

    // Called after the construction of an ActiveDOMObject to synchronize suspend state.
    void suspendActiveDOMObjectIfNeeded(ActiveDOMObject*);
#if !ENABLE(OILPAN)
    void ref() { refExecutionContext(); }
    void deref() { derefExecutionContext(); }
#endif

    // Gets the next id in a circular sequence from 1 to 2^31-1.
    int circularSequentialID();

    void didChangeTimerAlignmentInterval();

    SandboxFlags sandboxFlags() const { return m_sandboxFlags; }
    bool isSandboxed(SandboxFlags mask) const { return m_sandboxFlags & mask; }
    void enforceSandboxFlags(SandboxFlags mask);

    PassOwnPtr<LifecycleNotifier<ExecutionContext> > createLifecycleNotifier();

    virtual EventQueue* eventQueue() const = 0;

protected:
    ExecutionContext();
    virtual ~ExecutionContext();

    void setClient(ExecutionContextClient* client) { m_client = client; }

    virtual const KURL& virtualURL() const = 0;
    virtual KURL virtualCompleteURL(const String&) const = 0;

    ContextLifecycleNotifier& lifecycleNotifier();

private:
    friend class DOMTimer; // For installNewTimeout() and removeTimeoutByID() below.

    bool dispatchErrorEvent(PassRefPtrWillBeRawPtr<ErrorEvent>, AccessControlStatus);

#if !ENABLE(OILPAN)
    virtual void refExecutionContext() = 0;
    virtual void derefExecutionContext() = 0;
#endif
    // LifecycleContext implementation.

    // Implementation details for DOMTimer. No other classes should call these functions.
    int installNewTimeout(PassOwnPtr<ScheduledAction>, int timeout, bool singleShot);
    void removeTimeoutByID(int timeoutID); // This makes underlying DOMTimer instance destructed.

    ExecutionContextClient* m_client;
    SandboxFlags m_sandboxFlags;

    int m_circularSequentialID;
    typedef HashMap<int, OwnPtr<DOMTimer> > TimeoutMap;
    TimeoutMap m_timeouts;

    bool m_inDispatchErrorEvent;
    class PendingException;
    OwnPtrWillBeMember<WillBeHeapVector<OwnPtrWillBeMember<PendingException> > > m_pendingExceptions;

    bool m_activeDOMObjectsAreSuspended;
    bool m_activeDOMObjectsAreStopped;

    OwnPtr<PublicURLManager> m_publicURLManager;

    // The location of this member is important; to make sure contextDestroyed() notification on
    // ExecutionContext's members (notably m_timeouts) is called before they are destructed,
    // m_lifecycleNotifer should be placed *after* such members.
    OwnPtr<ContextLifecycleNotifier> m_lifecycleNotifier;
};

} // namespace WebCore

#endif // ExecutionContext_h
