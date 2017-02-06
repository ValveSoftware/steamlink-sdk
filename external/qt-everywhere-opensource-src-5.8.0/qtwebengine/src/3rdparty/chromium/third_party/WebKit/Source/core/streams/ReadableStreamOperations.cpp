// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

namespace {

bool isTerminating(ScriptState* scriptState)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    if (!executionContext)
        return true;
    if (!executionContext->isWorkerGlobalScope())
        return false;
    return toWorkerGlobalScope(executionContext)->scriptController()->isExecutionTerminating();
}

} // namespace

ScriptValue ReadableStreamOperations::createReadableStream(ScriptState* scriptState, UnderlyingSourceBase* underlyingSource, ScriptValue strategy)
{
    if (isTerminating(scriptState))
        return ScriptValue();
    ScriptState::Scope scope(scriptState);

    v8::Local<v8::Value> jsUnderlyingSource = toV8(underlyingSource, scriptState);
    v8::Local<v8::Value> jsStrategy = strategy.v8Value();
    v8::Local<v8::Value> args[] = { jsUnderlyingSource, jsStrategy };
    v8::MaybeLocal<v8::Value> jsStream = V8ScriptRunner::callExtra(scriptState, "createReadableStreamWithExternalController", args);
    if (isTerminating(scriptState))
        return ScriptValue();
    return ScriptValue(scriptState, v8CallOrCrash(jsStream));
}

ScriptValue ReadableStreamOperations::createCountQueuingStrategy(ScriptState* scriptState, size_t highWaterMark)
{
    if (isTerminating(scriptState))
        return ScriptValue();
    ScriptState::Scope scope(scriptState);

    v8::Local<v8::Value> args[] = { v8::Number::New(scriptState->isolate(), highWaterMark) };
    v8::MaybeLocal<v8::Value> jsStrategy = V8ScriptRunner::callExtra(scriptState, "createBuiltInCountQueuingStrategy", args);
    if (isTerminating(scriptState))
        return ScriptValue();

    return ScriptValue(scriptState, v8CallOrCrash(jsStrategy));
}

ScriptValue ReadableStreamOperations::getReader(ScriptState* scriptState, ScriptValue stream, ExceptionState& es)
{
    if (isTerminating(scriptState))
        return ScriptValue();
    ASSERT(isReadableStream(scriptState, stream));

    v8::TryCatch block(scriptState->isolate());
    v8::Local<v8::Value> args[] = { stream.v8Value() };
    ScriptValue result(scriptState, V8ScriptRunner::callExtra(scriptState, "AcquireReadableStreamDefaultReader", args));
    if (block.HasCaught())
        es.rethrowV8Exception(block.Exception());
    return result;
}

bool ReadableStreamOperations::isReadableStream(ScriptState* scriptState, ScriptValue value)
{
    if (isTerminating(scriptState))
        return true;
    ASSERT(!value.isEmpty());

    if (!value.isObject())
        return false;

    v8::Local<v8::Value> args[] = { value.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "IsReadableStream", args);
    if (isTerminating(scriptState))
        return true;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isDisturbed(ScriptState* scriptState, ScriptValue stream)
{
    if (isTerminating(scriptState))
        return true;
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "IsReadableStreamDisturbed", args);
    if (isTerminating(scriptState))
        return true;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isLocked(ScriptState* scriptState, ScriptValue stream)
{
    if (isTerminating(scriptState))
        return true;
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "IsReadableStreamLocked", args);
    if (isTerminating(scriptState))
        return true;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isReadable(ScriptState* scriptState, ScriptValue stream)
{
    if (isTerminating(scriptState))
        return false;
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStreamReadable", args);
    if (isTerminating(scriptState))
        return false;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isClosed(ScriptState* scriptState, ScriptValue stream)
{
    if (isTerminating(scriptState))
        return false;
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "IsReadableStreamClosed", args);
    if (isTerminating(scriptState))
        return false;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isErrored(ScriptState* scriptState, ScriptValue stream)
{
    if (isTerminating(scriptState))
        return true;
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "IsReadableStreamErrored", args);
    if (isTerminating(scriptState))
        return true;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isReadableStreamDefaultReader(ScriptState* scriptState, ScriptValue value)
{
    if (isTerminating(scriptState))
        return true;
    ASSERT(!value.isEmpty());

    if (!value.isObject())
        return false;

    v8::Local<v8::Value> args[] = { value.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "IsReadableStreamDefaultReader", args);
    if (isTerminating(scriptState))
        return true;
    return v8CallOrCrash(result)->ToBoolean()->Value();
}

ScriptPromise ReadableStreamOperations::defaultReaderRead(ScriptState* scriptState, ScriptValue reader)
{
    if (isTerminating(scriptState))
        return ScriptPromise();
    ASSERT(isReadableStreamDefaultReader(scriptState, reader));

    v8::Local<v8::Value> args[] = { reader.v8Value() };
    v8::MaybeLocal<v8::Value> result = V8ScriptRunner::callExtra(scriptState, "ReadableStreamDefaultReaderRead", args);
    if (isTerminating(scriptState))
        return ScriptPromise();
    return ScriptPromise::cast(scriptState, v8CallOrCrash(result));
}

void ReadableStreamOperations::tee(ScriptState* scriptState, ScriptValue stream, ScriptValue* newStream1, ScriptValue* newStream2)
{
    if (isTerminating(scriptState))
        return;
    DCHECK(isReadableStream(scriptState, stream));
    DCHECK(!isLocked(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };

    v8::MaybeLocal<v8::Value> maybeResult = V8ScriptRunner::callExtra(scriptState, "ReadableStreamTee", args);
    if (isTerminating(scriptState))
        return;
    ScriptValue result(scriptState, v8CallOrCrash(maybeResult));
    DCHECK(result.v8Value()->IsArray());
    v8::Local<v8::Array> branches = result.v8Value().As<v8::Array>();
    DCHECK_EQ(2u, branches->Length());

    v8::MaybeLocal<v8::Value> maybeStream1 = branches->Get(scriptState->context(), 0);
    if (isTerminating(scriptState))
        return;
    v8::MaybeLocal<v8::Value> maybeStream2 = branches->Get(scriptState->context(), 1);
    if (isTerminating(scriptState))
        return;

    *newStream1 = ScriptValue(scriptState, v8CallOrCrash(maybeStream1));
    *newStream2 = ScriptValue(scriptState, v8CallOrCrash(maybeStream2));

    DCHECK(isReadableStream(scriptState, *newStream1));
    DCHECK(isReadableStream(scriptState, *newStream2));
}

} // namespace blink
