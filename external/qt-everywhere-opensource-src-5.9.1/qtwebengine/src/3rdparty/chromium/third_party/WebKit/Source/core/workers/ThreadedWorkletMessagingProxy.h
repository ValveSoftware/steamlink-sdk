// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletMessagingProxy_h
#define ThreadedWorkletMessagingProxy_h

#include "core/CoreExport.h"
#include "core/workers/ThreadedMessagingProxyBase.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

class ThreadedWorkletObjectProxy;

class CORE_EXPORT ThreadedWorkletMessagingProxy
    : public ThreadedMessagingProxyBase,
      public WorkletGlobalScopeProxy {
 public:
  // WorkletGlobalScopeProxy implementation.
  void evaluateScript(const ScriptSourceCode&) final;
  void terminateWorkletGlobalScope() final;

  void initialize();

 protected:
  explicit ThreadedWorkletMessagingProxy(ExecutionContext*);

  ThreadedWorkletObjectProxy& workletObjectProxy() {
    return *m_workletObjectProxy;
  }

 private:
  std::unique_ptr<ThreadedWorkletObjectProxy> m_workletObjectProxy;
};

}  // namespace blink

#endif  // ThreadedWorkletMessagingProxy_h
