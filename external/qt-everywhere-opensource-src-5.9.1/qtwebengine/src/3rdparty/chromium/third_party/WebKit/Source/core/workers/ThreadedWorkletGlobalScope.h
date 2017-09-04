// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletGlobalScope_h
#define ThreadedWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/workers/WorkletGlobalScope.h"

namespace blink {

class WorkerThread;

class CORE_EXPORT ThreadedWorkletGlobalScope : public WorkletGlobalScope {
 public:
  ~ThreadedWorkletGlobalScope() override;
  void dispose() final;

  // ExecutionContext
  bool isThreadedWorkletGlobalScope() const final { return true; }
  bool isContextThread() const final;
  void postTask(const WebTraceLocation&,
                std::unique_ptr<ExecutionContextTask>,
                const String& taskNameForInstrumentation) final;
  void addConsoleMessage(ConsoleMessage*) final;
  void exceptionThrown(ErrorEvent*) final;

  WorkerThread* thread() const { return m_thread; }

 protected:
  ThreadedWorkletGlobalScope(const KURL&,
                             const String& userAgent,
                             PassRefPtr<SecurityOrigin>,
                             v8::Isolate*,
                             WorkerThread*);

 private:
  WorkerThread* m_thread;
};

DEFINE_TYPE_CASTS(ThreadedWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->isThreadedWorkletGlobalScope(),
                  context.isThreadedWorkletGlobalScope());

}  // namespace blink

#endif  // ThreadedWorkletGlobalScope_h
