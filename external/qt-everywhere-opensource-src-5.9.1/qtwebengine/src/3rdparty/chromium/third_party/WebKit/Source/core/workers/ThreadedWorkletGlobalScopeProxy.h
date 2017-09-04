// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletGlobalScopeProxy_h
#define ThreadedWorkletGlobalScopeProxy_h

#include "core/workers/WorkletGlobalScopeProxy.h"

// TODO(ikilpatrick): remove this file once AudioWorklet is no longer using it.

namespace blink {

class ThreadedWorkletGlobalScopeProxy : public WorkletGlobalScopeProxy {
 public:
  void evaluateScript(const ScriptSourceCode&) final {}
  void terminateWorkletGlobalScope() final {}
};

}  // namespace blink

#endif  // ThreadedWorkletGlobalScopeProxy_h
