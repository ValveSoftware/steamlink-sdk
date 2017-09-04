// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuspendableScriptExecutor_h
#define SuspendableScriptExecutor_h

#include "core/frame/SuspendableTimer.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "wtf/Vector.h"
#include <v8.h>

namespace blink {

class LocalFrame;
class ScriptSourceCode;
class ScriptState;
class WebScriptExecutionCallback;

class SuspendableScriptExecutor final
    : public GarbageCollectedFinalized<SuspendableScriptExecutor>,
      public SuspendableTimer {
  USING_GARBAGE_COLLECTED_MIXIN(SuspendableScriptExecutor);

 public:
  static void createAndRun(LocalFrame*,
                           int worldID,
                           const HeapVector<ScriptSourceCode>& sources,
                           int extensionGroup,
                           bool userGesture,
                           WebScriptExecutionCallback*);
  static void createAndRun(LocalFrame*,
                           v8::Isolate*,
                           v8::Local<v8::Context>,
                           v8::Local<v8::Function>,
                           v8::Local<v8::Value> receiver,
                           int argc,
                           v8::Local<v8::Value> argv[],
                           WebScriptExecutionCallback*);
  ~SuspendableScriptExecutor() override;

  void contextDestroyed() override;

  DECLARE_VIRTUAL_TRACE();

  class Executor : public GarbageCollectedFinalized<Executor> {
   public:
    virtual ~Executor() {}

    virtual Vector<v8::Local<v8::Value>> execute(LocalFrame*) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE(){};
  };

 private:
  SuspendableScriptExecutor(LocalFrame*,
                            ScriptState*,
                            WebScriptExecutionCallback*,
                            Executor*);

  void fired() override;

  void run();
  void executeAndDestroySelf();
  void dispose();

  RefPtr<ScriptState> m_scriptState;
  WebScriptExecutionCallback* m_callback;

  SelfKeepAlive<SuspendableScriptExecutor> m_keepAlive;

  Member<Executor> m_executor;
};

}  // namespace blink

#endif  // SuspendableScriptExecutor_h
