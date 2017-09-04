/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InProcessWorkerObjectProxy_h
#define InProcessWorkerObjectProxy_h

#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include <memory>

namespace blink {

class ConsoleMessage;
class ExecutionContext;
class ExecutionContextTask;
class InProcessWorkerMessagingProxy;
class ParentFrameTaskRunners;
class WorkerGlobalScope;
class WorkerOrWorkletGlobalScope;

// A proxy to talk to the worker object. This object is created on the
// parent context thread (i.e. usually the main thread), passed on to
// the worker thread, and used to proxy messages to the
// InProcessWorkerMessagingProxy on the parent context thread.
//
// This also checks pending activities on WorkerGlobalScope and reports a result
// to the message proxy when an exponential backoff timer is fired.
//
// Used only by in-process workers (DedicatedWorker and CompositorWorker.)
class CORE_EXPORT InProcessWorkerObjectProxy : public WorkerReportingProxy {
  USING_FAST_MALLOC(InProcessWorkerObjectProxy);
  WTF_MAKE_NONCOPYABLE(InProcessWorkerObjectProxy);

 public:
  static std::unique_ptr<InProcessWorkerObjectProxy> create(
      const WeakPtr<InProcessWorkerMessagingProxy>&);
  ~InProcessWorkerObjectProxy() override;

  void postMessageToWorkerObject(PassRefPtr<SerializedScriptValue>,
                                 std::unique_ptr<MessagePortChannelArray>);
  void postTaskToMainExecutionContext(std::unique_ptr<ExecutionContextTask>);
  void confirmMessageFromWorkerObject();
  void startPendingActivityTimer();

  // WorkerReportingProxy overrides.
  void reportException(const String& errorMessage,
                       std::unique_ptr<SourceLocation>,
                       int exceptionId) override;
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void postMessageToPageInspector(const String&) override;
  void didCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void didEvaluateWorkerScript(bool success) override;
  void didCloseWorkerGlobalScope() override;
  void willDestroyWorkerGlobalScope() override;
  void didTerminateWorkerThread() override;

 protected:
  InProcessWorkerObjectProxy(const WeakPtr<InProcessWorkerMessagingProxy>&);
  virtual ExecutionContext* getExecutionContext();

 private:
  friend class InProcessWorkerMessagingProxyForTest;

  void checkPendingActivity(TimerBase*);

  // Returns the parent frame's task runners.
  ParentFrameTaskRunners* getParentFrameTaskRunners();

  // This object always outlives this proxy.
  InProcessWorkerMessagingProxy* m_messagingProxy;

  // No guarantees about the lifetimes of tasks posted by this proxy wrt the
  // InProcessWorkerMessagingProxy so a weak pointer must be used when posting
  // the tasks.
  WeakPtr<InProcessWorkerMessagingProxy> m_messagingProxyWeakPtr;

  // Used for checking pending activities on the worker global scope. This is
  // cancelled when the worker global scope is destroyed.
  std::unique_ptr<Timer<InProcessWorkerObjectProxy>> m_timer;

  // The default interval duration of the timer. This is usually
  // kDefaultIntervalInSec but made as a member variable for testing.
  double m_defaultIntervalInSec;

  // The next interval duration of the timer. This is initially set to
  // |m_defaultIntervalInSec| and exponentially increased up to
  // |m_maxIntervalInSec|.
  double m_nextIntervalInSec;

  // The max interval duration of the timer. This is usually kMaxIntervalInSec
  // but made as a member variable for testing.
  double m_maxIntervalInSec;

  CrossThreadPersistent<WorkerGlobalScope> m_workerGlobalScope;
};

}  // namespace blink

#endif  // InProcessWorkerObjectProxy_h
