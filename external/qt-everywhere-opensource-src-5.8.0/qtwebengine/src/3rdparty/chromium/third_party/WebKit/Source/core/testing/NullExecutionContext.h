// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NullExecutionContext_h
#define NullExecutionContext_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/events/EventQueue.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include <memory>

namespace blink {

class NullExecutionContext final : public GarbageCollectedFinalized<NullExecutionContext>, public SecurityContext, public ExecutionContext {
    USING_GARBAGE_COLLECTED_MIXIN(NullExecutionContext);
public:
    NullExecutionContext();

    void disableEval(const String&) override { }
    String userAgent() const override { return String(); }

    void postTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>, const String& taskNameForInstrumentation = emptyString()) override;

    EventTarget* errorEventTarget() override { return nullptr; }
    EventQueue* getEventQueue() const override { return m_queue.get(); }

    bool tasksNeedSuspension() override { return m_tasksNeedSuspension; }
    void setTasksNeedSuspension(bool flag) { m_tasksNeedSuspension = flag; }

    void reportBlockedScriptExecutionToInspector(const String& directiveText) override { }
    void didUpdateSecurityOrigin() override { }
    SecurityContext& securityContext() override { return *this; }
    DOMTimerCoordinator* timers() override { return nullptr; }

    void addConsoleMessage(ConsoleMessage*) override { }
    void logExceptionToConsole(const String& errorMessage, std::unique_ptr<SourceLocation>) override { }

    void setIsSecureContext(bool);
    bool isSecureContext(String& errorMessage, const SecureContextCheck = StandardSecureContextCheck) const override;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_queue);
        SecurityContext::trace(visitor);
        ExecutionContext::trace(visitor);
    }

protected:
    const KURL& virtualURL() const override { return m_dummyURL; }
    KURL virtualCompleteURL(const String&) const override { return m_dummyURL; }

private:
    bool m_tasksNeedSuspension;
    bool m_isSecureContext;
    Member<EventQueue> m_queue;

    KURL m_dummyURL;
};

} // namespace blink

#endif // NullExecutionContext_h
