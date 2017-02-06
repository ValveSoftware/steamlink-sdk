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

#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/fetch/CachedMetadataHandler.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/frame/DOMWindowBase64.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerEventQueue.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"
#include <memory>

namespace blink {

class ConsoleMessage;
class ExceptionState;
class V8AbstractEventListener;
class WorkerClients;
class WorkerInspectorController;
class WorkerLocation;
class WorkerNavigator;
class WorkerThread;

class CORE_EXPORT WorkerGlobalScope : public EventTargetWithInlineData, public SecurityContext, public WorkerOrWorkletGlobalScope, public Supplementable<WorkerGlobalScope>, public DOMWindowBase64 {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(WorkerGlobalScope);
public:
    ~WorkerGlobalScope() override;

    bool isWorkerGlobalScope() const final { return true; }

    ExecutionContext* getExecutionContext() const final;
    ScriptWrappable* getScriptWrappable() const final
    {
        return const_cast<WorkerGlobalScope*>(this);
    }

    virtual void countFeature(UseCounter::Feature) const;
    virtual void countDeprecation(UseCounter::Feature) const;

    const KURL& url() const { return m_url; }
    KURL completeURL(const String&) const;

    String userAgent() const final;
    void disableEval(const String& errorMessage) final;

    WorkerOrWorkletScriptController* scriptController() final { return m_scriptController.get(); }

    virtual void didEvaluateWorkerScript();
    void dispose();

    WorkerThread* thread() const { return m_thread; }

    void postTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>, const String& taskNameForInstrumentation) final; // Executes the task on context's thread asynchronously.

    // WorkerGlobalScope
    WorkerGlobalScope* self() { return this; }
    WorkerLocation* location() const;
    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(rejectionhandled);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(unhandledrejection);

    // WorkerUtils
    virtual void importScripts(const Vector<String>& urls, ExceptionState&);
    // Returns null if caching is not supported.
    virtual CachedMetadataHandler* createWorkerScriptCachedMetadataHandler(const KURL& scriptURL, const Vector<char>* metaData) { return nullptr; }

    WorkerNavigator* navigator() const;

    // ScriptWrappable
    v8::Local<v8::Object> wrap(v8::Isolate*, v8::Local<v8::Object> creationContext) final;
    v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) final;

    // ExecutionContext
    WorkerEventQueue* getEventQueue() const final;
    SecurityContext& securityContext() final { return *this; }

    bool isContextThread() const final;
    bool isJSExecutionForbidden() const final;

    DOMTimerCoordinator* timers() final;

    WorkerInspectorController* workerInspectorController() { return m_workerInspectorController.get(); }

    // Returns true when the WorkerGlobalScope is closing (e.g. via close()
    // method). If this returns true, the worker is going to be shutdown after
    // the current task execution. Workers that don't support close operation
    // should always return false.
    bool isClosing() const { return m_closing; }

    double timeOrigin() const { return m_timeOrigin; }

    WorkerClients* clients() { return m_workerClients.get(); }

    using SecurityContext::getSecurityOrigin;
    using SecurityContext::contentSecurityPolicy;

    void addConsoleMessage(ConsoleMessage*) final;

    void exceptionUnhandled(const String& errorMessage, std::unique_ptr<SourceLocation>);

    virtual void scriptLoaded(size_t scriptSize, size_t cachedMetadataSize) { }

    bool isSecureContext(String& errorMessage, const SecureContextCheck = StandardSecureContextCheck) const override;

    void registerEventListener(V8AbstractEventListener*);
    void deregisterEventListener(V8AbstractEventListener*);

    DECLARE_VIRTUAL_TRACE();

protected:
    WorkerGlobalScope(const KURL&, const String& userAgent, WorkerThread*, double timeOrigin, std::unique_ptr<SecurityOrigin::PrivilegeData>, WorkerClients*);
    void applyContentSecurityPolicyFromVector(const Vector<CSPHeaderAndType>& headers);

    void logExceptionToConsole(const String& errorMessage, std::unique_ptr<SourceLocation>) override;
    void addMessageToWorkerConsole(ConsoleMessage*);
    void setV8CacheOptions(V8CacheOptions v8CacheOptions) { m_v8CacheOptions = v8CacheOptions; }

    void removeURLFromMemoryCache(const KURL&) override;

private:
    const KURL& virtualURL() const final;
    KURL virtualCompleteURL(const String&) const final;

    void reportBlockedScriptExecutionToInspector(const String& directiveText) final;

    EventTarget* errorEventTarget() final;
    void didUpdateSecurityOrigin() final { }

    void clearScript();
    void clearInspector();

    static void removeURLFromMemoryCacheInternal(const KURL&);

    KURL m_url;
    String m_userAgent;
    V8CacheOptions m_v8CacheOptions;

    mutable Member<WorkerLocation> m_location;
    mutable Member<WorkerNavigator> m_navigator;

    mutable UseCounter::CountBits m_deprecationWarningBits;

    Member<WorkerOrWorkletScriptController> m_scriptController;
    WorkerThread* m_thread;

    Member<WorkerInspectorController> m_workerInspectorController;
    bool m_closing;

    Member<WorkerEventQueue> m_eventQueue;

    Member<WorkerClients> m_workerClients;

    DOMTimerCoordinator m_timers;

    double m_timeOrigin;

    HeapListHashSet<Member<V8AbstractEventListener>> m_eventListeners;
};

DEFINE_TYPE_CASTS(WorkerGlobalScope, ExecutionContext, context, context->isWorkerGlobalScope(), context.isWorkerGlobalScope());

} // namespace blink

#endif // WorkerGlobalScope_h
