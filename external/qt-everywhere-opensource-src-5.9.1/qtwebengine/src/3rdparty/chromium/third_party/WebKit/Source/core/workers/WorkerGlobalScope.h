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

#include "bindings/core/v8/ActiveScriptWrappable.h"
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
#include "core/workers/WorkerSettings.h"
#include "platform/heap/Handle.h"
#include "wtf/ListHashSet.h"
#include <memory>

namespace blink {

class ConsoleMessage;
class ExceptionState;
class V8AbstractEventListener;
class WorkerClients;
class WorkerLocation;
class WorkerNavigator;
class WorkerThread;

class CORE_EXPORT WorkerGlobalScope : public EventTargetWithInlineData,
                                      public ActiveScriptWrappable,
                                      public SecurityContext,
                                      public WorkerOrWorkletGlobalScope,
                                      public Supplementable<WorkerGlobalScope>,
                                      public DOMWindowBase64 {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkerGlobalScope);

 public:
  using SecurityContext::getSecurityOrigin;
  using SecurityContext::contentSecurityPolicy;

  ~WorkerGlobalScope() override;

  virtual void countFeature(UseCounter::Feature) const;
  virtual void countDeprecation(UseCounter::Feature) const;

  // Returns null if caching is not supported.
  virtual CachedMetadataHandler* createWorkerScriptCachedMetadataHandler(
      const KURL& scriptURL,
      const Vector<char>* metaData) {
    return nullptr;
  }

  KURL completeURL(const String&) const;

  // WorkerOrWorkletGlobalScope
  bool isClosing() const final { return m_closing; }
  virtual void dispose();
  WorkerThread* thread() const final { return m_thread; }

  void exceptionUnhandled(int exceptionId);

  void registerEventListener(V8AbstractEventListener*);
  void deregisterEventListener(V8AbstractEventListener*);

  // WorkerGlobalScope
  WorkerGlobalScope* self() { return this; }
  WorkerLocation* location() const;
  WorkerNavigator* navigator() const;
  void close();
  bool isSecureContextForBindings() const {
    return ExecutionContext::isSecureContext(StandardSecureContextCheck);
  }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(rejectionhandled);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unhandledrejection);

  // WorkerUtils
  virtual void importScripts(const Vector<String>& urls, ExceptionState&);

  // ScriptWrappable
  v8::Local<v8::Object> wrap(v8::Isolate*,
                             v8::Local<v8::Object> creationContext) final;
  v8::Local<v8::Object> associateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;

  // ScriptWrappable
  bool hasPendingActivity() const override;

  // ExecutionContext
  bool isWorkerGlobalScope() const final { return true; }
  bool isJSExecutionForbidden() const final;
  bool isContextThread() const final;
  void disableEval(const String& errorMessage) final;
  String userAgent() const final { return m_userAgent; }
  void postTask(const WebTraceLocation&,
                std::unique_ptr<ExecutionContextTask>,
                const String& taskNameForInstrumentation) final;
  DOMTimerCoordinator* timers() final { return &m_timers; }
  SecurityContext& securityContext() final { return *this; }
  void addConsoleMessage(ConsoleMessage*) final;
  WorkerEventQueue* getEventQueue() const final;
  bool isSecureContext(
      String& errorMessage,
      const SecureContextCheck = StandardSecureContextCheck) const override;

  // EventTarget
  ExecutionContext* getExecutionContext() const final;

  // WorkerOrWorkletGlobalScope
  ScriptWrappable* getScriptWrappable() const final {
    return const_cast<WorkerGlobalScope*>(this);
  }

  double timeOrigin() const { return m_timeOrigin; }
  WorkerSettings* workerSettings() const { return m_workerSettings.get(); }

  WorkerOrWorkletScriptController* scriptController() final {
    return m_scriptController.get();
  }
  WorkerClients* clients() { return m_workerClients.get(); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  WorkerGlobalScope(const KURL&,
                    const String& userAgent,
                    WorkerThread*,
                    double timeOrigin,
                    std::unique_ptr<SecurityOrigin::PrivilegeData>,
                    WorkerClients*);
  void setWorkerSettings(std::unique_ptr<WorkerSettings>);
  void applyContentSecurityPolicyFromVector(
      const Vector<CSPHeaderAndType>& headers);

  void setV8CacheOptions(V8CacheOptions v8CacheOptions) {
    m_v8CacheOptions = v8CacheOptions;
  }

  // ExecutionContext
  void exceptionThrown(ErrorEvent*) override;
  void removeURLFromMemoryCache(const KURL&) final;

 private:
  // ExecutionContext
  EventTarget* errorEventTarget() final { return this; }
  const KURL& virtualURL() const final { return m_url; }
  KURL virtualCompleteURL(const String&) const final;

  // SecurityContext
  void didUpdateSecurityOrigin() final {}

  const KURL m_url;
  const String m_userAgent;
  V8CacheOptions m_v8CacheOptions;
  std::unique_ptr<WorkerSettings> m_workerSettings;

  mutable Member<WorkerLocation> m_location;
  mutable Member<WorkerNavigator> m_navigator;

  mutable BitVector m_deprecationWarningBits;

  Member<WorkerOrWorkletScriptController> m_scriptController;
  WorkerThread* m_thread;

  bool m_closing;

  Member<WorkerEventQueue> m_eventQueue;

  CrossThreadPersistent<WorkerClients> m_workerClients;

  DOMTimerCoordinator m_timers;

  const double m_timeOrigin;

  HeapListHashSet<Member<V8AbstractEventListener>> m_eventListeners;

  HeapHashMap<int, Member<ErrorEvent>> m_pendingErrorEvents;
  int m_lastPendingErrorEventId;
};

DEFINE_TYPE_CASTS(WorkerGlobalScope,
                  ExecutionContext,
                  context,
                  context->isWorkerGlobalScope(),
                  context.isWorkerGlobalScope());

}  // namespace blink

#endif  // WorkerGlobalScope_h
