/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef WorkerGlobalScope_h
#define WorkerGlobalScope_h

#include "bindings/v8/ScriptWrappable.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowBase64.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerEventQueue.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

    class Blob;
    class ExceptionState;
    class ScheduledAction;
    class WorkerClients;
    class WorkerConsole;
    class WorkerInspectorController;
    class WorkerLocation;
    class WorkerNavigator;
    class WorkerThread;

    class WorkerGlobalScope : public RefCountedWillBeRefCountedGarbageCollected<WorkerGlobalScope>, public ScriptWrappable, public SecurityContext, public ExecutionContext, public ExecutionContextClient, public WillBeHeapSupplementable<WorkerGlobalScope>, public EventTargetWithInlineData, public DOMWindowBase64 {
        WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WorkerGlobalScope);
        REFCOUNTED_EVENT_TARGET(WorkerGlobalScope);
    public:
        virtual ~WorkerGlobalScope();

        virtual bool isWorkerGlobalScope() const OVERRIDE FINAL { return true; }

        virtual ExecutionContext* executionContext() const OVERRIDE FINAL;

        virtual bool isSharedWorkerGlobalScope() const { return false; }
        virtual bool isDedicatedWorkerGlobalScope() const { return false; }
        virtual bool isServiceWorkerGlobalScope() const { return false; }
        virtual void countFeature(UseCounter::Feature) const;
        virtual void countDeprecation(UseCounter::Feature) const;

        const KURL& url() const { return m_url; }
        KURL completeURL(const String&) const;

        virtual String userAgent(const KURL&) const OVERRIDE FINAL;
        virtual void disableEval(const String& errorMessage) OVERRIDE FINAL;

        WorkerScriptController* script() { return m_script.get(); }
        void clearScript() { m_script.clear(); }
        void clearInspector();

        // FIXME: We can remove this interface when we remove openDatabaseSync.
        class TerminationObserver {
        public:
            virtual ~TerminationObserver() { }
            // The function is probably called in the main thread.
            virtual void wasRequestedToTerminate() = 0;
        };
        void registerTerminationObserver(TerminationObserver*);
        void unregisterTerminationObserver(TerminationObserver*);
        void wasRequestedToTerminate();

        void dispose();

        WorkerThread* thread() const { return m_thread; }

        virtual void postTask(PassOwnPtr<ExecutionContextTask>) OVERRIDE FINAL; // Executes the task on context's thread asynchronously.

        // WorkerGlobalScope
        WorkerGlobalScope* self() { return this; }
        WorkerConsole* console();
        WorkerLocation* location() const;
        void close();

        DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

        // WorkerUtils
        virtual void importScripts(const Vector<String>& urls, ExceptionState&);
        WorkerNavigator* navigator() const;

        // ExecutionContextClient
        virtual WorkerEventQueue* eventQueue() const OVERRIDE FINAL;
        virtual SecurityContext& securityContext() OVERRIDE FINAL { return *this; }

        virtual bool isContextThread() const OVERRIDE FINAL;
        virtual bool isJSExecutionForbidden() const OVERRIDE FINAL;

        virtual double timerAlignmentInterval() const OVERRIDE FINAL;

        WorkerInspectorController* workerInspectorController() { return m_workerInspectorController.get(); }

        bool isClosing() { return m_closing; }

        virtual void stopFetch() { }

        bool idleNotification();

        double timeOrigin() const { return m_timeOrigin; }

        WorkerClients* clients() { return m_workerClients.get(); }

        using SecurityContext::securityOrigin;
        using SecurityContext::contentSecurityPolicy;

        virtual void trace(Visitor*) OVERRIDE;

    protected:
        WorkerGlobalScope(const KURL&, const String& userAgent, WorkerThread*, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerClients>);
        void applyContentSecurityPolicyFromString(const String& contentSecurityPolicy, ContentSecurityPolicyHeaderType);

        virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>) OVERRIDE;
        void addMessageToWorkerConsole(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>, ScriptState*);

    private:
#if !ENABLE(OILPAN)
        virtual void refExecutionContext() OVERRIDE FINAL { ref(); }
        virtual void derefExecutionContext() OVERRIDE FINAL { deref(); }
#endif

        virtual const KURL& virtualURL() const OVERRIDE FINAL;
        virtual KURL virtualCompleteURL(const String&) const OVERRIDE FINAL;

        virtual void reportBlockedScriptExecutionToInspector(const String& directiveText) OVERRIDE FINAL;
        virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState* = 0) OVERRIDE FINAL;

        virtual EventTarget* errorEventTarget() OVERRIDE FINAL;
        virtual void didUpdateSecurityOrigin() OVERRIDE FINAL { }

        KURL m_url;
        String m_userAgent;

        mutable RefPtrWillBeMember<WorkerConsole> m_console;
        mutable RefPtrWillBeMember<WorkerLocation> m_location;
        mutable RefPtrWillBeMember<WorkerNavigator> m_navigator;

        OwnPtr<WorkerScriptController> m_script;
        WorkerThread* m_thread;

        OwnPtr<WorkerInspectorController> m_workerInspectorController;
        bool m_closing;

        OwnPtrWillBeMember<WorkerEventQueue> m_eventQueue;

        OwnPtrWillBeMember<WorkerClients> m_workerClients;

        double m_timeOrigin;
        TerminationObserver* m_terminationObserver;
    };

DEFINE_TYPE_CASTS(WorkerGlobalScope, ExecutionContext, context, context->isWorkerGlobalScope(), context.isWorkerGlobalScope());

} // namespace WebCore

#endif // WorkerGlobalScope_h
