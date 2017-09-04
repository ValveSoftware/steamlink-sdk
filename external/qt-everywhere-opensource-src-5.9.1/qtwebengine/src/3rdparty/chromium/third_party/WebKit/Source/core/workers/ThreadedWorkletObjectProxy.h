// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletObjectProxy_h
#define ThreadedWorkletObjectProxy_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerReportingProxy.h"

namespace blink {

class ConsoleMessage;
class ExecutionContext;
class ExecutionContextTask;
class ThreadedWorkletMessagingProxy;

// A proxy to talk to the parent worklet object. This object is created on the
// main thread, passed on to the worklet thread, and used just to proxy
// messages to the ThreadedWorkletMessagingProxy on the main thread.
class CORE_EXPORT ThreadedWorkletObjectProxy : public WorkerReportingProxy {
  USING_FAST_MALLOC(ThreadedWorkletObjectProxy);
  WTF_MAKE_NONCOPYABLE(ThreadedWorkletObjectProxy);

 public:
  static std::unique_ptr<ThreadedWorkletObjectProxy> create(
      ThreadedWorkletMessagingProxy*);
  ~ThreadedWorkletObjectProxy() override {}

  void postTaskToMainExecutionContext(std::unique_ptr<ExecutionContextTask>);
  void reportPendingActivity(bool hasPendingActivity);

  // WorkerReportingProxy overrides.
  void reportException(const String& errorMessage,
                       std::unique_ptr<SourceLocation>,
                       int exceptionId) override {}
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void postMessageToPageInspector(const String&) override;
  void didEvaluateWorkerScript(bool success) override {}
  void didCloseWorkerGlobalScope() override;
  void willDestroyWorkerGlobalScope() override {}
  void didTerminateWorkerThread() override;

 protected:
  ThreadedWorkletObjectProxy(ThreadedWorkletMessagingProxy*);

 private:
  ExecutionContext* getExecutionContext() const;

  // This object always outlives this proxy.
  ThreadedWorkletMessagingProxy* m_messagingProxy;
};

}  // namespace blink

#endif  // ThreadedWorkletObjectProxy_h
