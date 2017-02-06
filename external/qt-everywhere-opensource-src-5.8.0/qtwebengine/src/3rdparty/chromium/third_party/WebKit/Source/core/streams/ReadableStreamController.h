// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamController_h
#define ReadableStreamController_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/CoreExport.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {

// TODO(tyoshino): Rename this to ReadableStreamDefaultControllerWrapper.
class CORE_EXPORT ReadableStreamController final : public GarbageCollectedFinalized<ReadableStreamController> {
public:
    DEFINE_INLINE_TRACE() {}

    explicit ReadableStreamController(ScriptValue controller)
        : m_scriptState(controller.getScriptState())
        , m_jsController(controller.isolate(), controller.v8Value())
    {
        m_jsController.setWeak(&m_jsController, ReadableStreamController::controllerWeakCallback);
    }

    // Users of the ReadableStreamController can call this to note that the stream has been canceled and thus they
    // don't anticipate using the ReadableStreamController anymore. (close/desiredSize/enqueue/error will become no-ops
    // afterward.)
    void noteHasBeenCanceled()
    {
        m_jsController.clear();
    }

    bool isActive() const
    {
        return !m_jsController.isEmpty();
    }

    void close()
    {
        if (isTerminating(m_scriptState.get())) {
            m_jsController.clear();
            return;
        }
        ScriptState* scriptState = m_scriptState.get();
        ScriptState::Scope scope(scriptState); // will assert context is valid; do not call this method when the context is invalidated
        v8::Isolate* isolate = scriptState->isolate();

        v8::Local<v8::Value> controller = m_jsController.newLocal(isolate);
        if (controller.IsEmpty())
            return;

        v8::Local<v8::Value> args[] = { controller };
        v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "ReadableStreamDefaultControllerClose", args);
        m_jsController.clear();
        if (isTerminating(m_scriptState.get()))
            return;
        v8CallOrCrash(result);
    }

    double desiredSize() const
    {
        if (isTerminating(m_scriptState.get()))
            return 0;
        ScriptState* scriptState = m_scriptState.get();
        ScriptState::Scope scope(scriptState); // will assert context is valid; do not call this method when the context is invalidated
        v8::Isolate* isolate = scriptState->isolate();

        v8::Local<v8::Value> controller = m_jsController.newLocal(isolate);
        if (controller.IsEmpty())
            return 0;

        v8::Local<v8::Value> args[] = { controller };
        v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "ReadableStreamDefaultControllerGetDesiredSize", args);
        if (isTerminating(m_scriptState.get()))
            return 0;

        return v8CallOrCrash(result).As<v8::Number>()->Value();
    }

    template <typename ChunkType>
    void enqueue(ChunkType chunk) const
    {
        if (isTerminating(m_scriptState.get()))
            return;
        ScriptState* scriptState = m_scriptState.get();
        ScriptState::Scope scope(scriptState); // will assert context is valid; do not call this method when the context is invalidated
        v8::Isolate* isolate = scriptState->isolate();

        v8::Local<v8::Value> controller = m_jsController.newLocal(isolate);
        if (controller.IsEmpty())
            return;

        v8::Local<v8::Value> jsChunk = toV8(chunk, scriptState);
        v8::Local<v8::Value> args[] = { controller, jsChunk };
        v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "ReadableStreamDefaultControllerEnqueue", args);
        if (isTerminating(m_scriptState.get()))
            return;
        v8CallOrCrash(result);
    }

    template <typename ErrorType>
    void error(ErrorType error)
    {
        if (isTerminating(m_scriptState.get())) {
            m_jsController.clear();
            return;
        }
        ScriptState* scriptState = m_scriptState.get();
        ScriptState::Scope scope(scriptState); // will assert context is valid; do not call this method when the context is invalidated
        v8::Isolate* isolate = scriptState->isolate();

        v8::Local<v8::Value> controller = m_jsController.newLocal(isolate);
        if (controller.IsEmpty())
            return;

        v8::Local<v8::Value> jsError = toV8(error, scriptState);
        v8::Local<v8::Value> args[] = { controller, jsError };
        v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "ReadableStreamDefaultControllerError", args);
        m_jsController.clear();
        if (isTerminating(m_scriptState.get()))
            return;
        v8CallOrCrash(result);
    }

private:
    static void controllerWeakCallback(const v8::WeakCallbackInfo<ScopedPersistent<v8::Value>>& weakInfo)
    {
        weakInfo.GetParameter()->clear();
    }

    static bool isTerminating(ScriptState* scriptState)
    {
        ExecutionContext* executionContext = scriptState->getExecutionContext();
        if (!executionContext)
            return true;
        if (!executionContext->isWorkerGlobalScope())
            return false;
        return toWorkerGlobalScope(executionContext)->scriptController()->isExecutionTerminating();
    }

    RefPtr<ScriptState> m_scriptState;
    ScopedPersistent<v8::Value> m_jsController;
};

} // namespace blink

#endif // ReadableStreamController_h
