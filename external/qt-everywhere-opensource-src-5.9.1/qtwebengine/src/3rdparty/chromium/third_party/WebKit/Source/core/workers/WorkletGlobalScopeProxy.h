// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScopeProxy_h
#define WorkletGlobalScopeProxy_h

#include "core/CoreExport.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptSourceCode;

// A proxy to talk to the worklet global scope. The global scope may exist in
// the main thread or on a different thread.
class CORE_EXPORT WorkletGlobalScopeProxy {
 public:
  virtual ~WorkletGlobalScopeProxy() {}

  virtual void evaluateScript(const ScriptSourceCode&) = 0;
  virtual void terminateWorkletGlobalScope() = 0;
};

}  // namespace blink

#endif  // WorkletGlobalScopeProxy_h
