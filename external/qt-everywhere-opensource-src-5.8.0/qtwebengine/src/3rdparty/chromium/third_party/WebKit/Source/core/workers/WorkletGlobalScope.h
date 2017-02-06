// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScope_h
#define WorkletGlobalScope_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class EventQueue;
class WorkerOrWorkletScriptController;

class CORE_EXPORT WorkletGlobalScope : public GarbageCollectedFinalized<WorkletGlobalScope>, public SecurityContext, public WorkerOrWorkletGlobalScope, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);
public:
    ~WorkletGlobalScope() override;
    virtual void dispose();

    bool isWorkletGlobalScope() const final { return true; }

    // WorkerOrWorkletGlobalScope
    ScriptWrappable* getScriptWrappable() const final { return const_cast<WorkletGlobalScope*>(this); }
    WorkerOrWorkletScriptController* scriptController() final { return m_scriptController.get(); }

    // ScriptWrappable
    v8::Local<v8::Object> wrap(v8::Isolate*, v8::Local<v8::Object> creationContext) final;
    v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) final;

    // ExecutionContext
    void disableEval(const String& errorMessage) final;
    String userAgent() const final { return m_userAgent; }
    SecurityContext& securityContext() final { return *this; }
    EventQueue* getEventQueue() const final { NOTREACHED(); return nullptr; } // WorkletGlobalScopes don't have an event queue.
    bool isSecureContext(String& errorMessage, const SecureContextCheck = StandardSecureContextCheck) const final;

    using SecurityContext::getSecurityOrigin;
    using SecurityContext::contentSecurityPolicy;

    DOMTimerCoordinator* timers() final { NOTREACHED(); return nullptr; } // WorkletGlobalScopes don't have timers.
    void postTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>, const String&) override
    {
        // TODO(ikilpatrick): implement.
        NOTREACHED();
    }

    void reportBlockedScriptExecutionToInspector(const String& directiveText) final;
    void logExceptionToConsole(const String& errorMessage, std::unique_ptr<SourceLocation>) final;

    DECLARE_VIRTUAL_TRACE();

protected:
    // The url, userAgent and securityOrigin arguments are inherited from the
    // parent ExecutionContext for Worklets.
    WorkletGlobalScope(const KURL&, const String& userAgent, PassRefPtr<SecurityOrigin>, v8::Isolate*);

private:
    const KURL& virtualURL() const final { return m_url; }
    KURL virtualCompleteURL(const String&) const final;

    EventTarget* errorEventTarget() final { return nullptr; }
    void didUpdateSecurityOrigin() final { }

    KURL m_url;
    String m_userAgent;
    Member<WorkerOrWorkletScriptController> m_scriptController;
};

DEFINE_TYPE_CASTS(WorkletGlobalScope, ExecutionContext, context, context->isWorkletGlobalScope(), context.isWorkletGlobalScope());

} // namespace blink

#endif // WorkletGlobalScope_h
