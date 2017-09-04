// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/streams/UnderlyingSourceBase.h"

namespace blink {

ScriptValue ReadableStreamOperations::createReadableStream(
    ScriptState* scriptState,
    UnderlyingSourceBase* underlyingSource,
    ScriptValue strategy) {
  ScriptState::Scope scope(scriptState);

  v8::Local<v8::Value> jsUnderlyingSource = toV8(underlyingSource, scriptState);
  v8::Local<v8::Value> jsStrategy = strategy.v8Value();
  v8::Local<v8::Value> args[] = {jsUnderlyingSource, jsStrategy};
  return ScriptValue(
      scriptState,
      V8ScriptRunner::callExtraOrCrash(
          scriptState, "createReadableStreamWithExternalController", args));
}

ScriptValue ReadableStreamOperations::createCountQueuingStrategy(
    ScriptState* scriptState,
    size_t highWaterMark) {
  ScriptState::Scope scope(scriptState);

  v8::Local<v8::Value> args[] = {
      v8::Number::New(scriptState->isolate(), highWaterMark)};
  return ScriptValue(
      scriptState, V8ScriptRunner::callExtraOrCrash(
                       scriptState, "createBuiltInCountQueuingStrategy", args));
}

ScriptValue ReadableStreamOperations::getReader(ScriptState* scriptState,
                                                ScriptValue stream,
                                                ExceptionState& es) {
  ASSERT(isReadableStream(scriptState, stream));

  v8::TryCatch block(scriptState->isolate());
  v8::Local<v8::Value> args[] = {stream.v8Value()};
  ScriptValue result(
      scriptState,
      V8ScriptRunner::callExtra(scriptState,
                                "AcquireReadableStreamDefaultReader", args));
  if (block.HasCaught())
    es.rethrowV8Exception(block.Exception());
  return result;
}

bool ReadableStreamOperations::isReadableStream(ScriptState* scriptState,
                                                ScriptValue value) {
  ASSERT(!value.isEmpty());

  if (!value.isObject())
    return false;

  v8::Local<v8::Value> args[] = {value.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStream", args)
      ->ToBoolean()
      ->Value();
}

bool ReadableStreamOperations::isDisturbed(ScriptState* scriptState,
                                           ScriptValue stream) {
  ASSERT(isReadableStream(scriptState, stream));

  v8::Local<v8::Value> args[] = {stream.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState,
                                          "IsReadableStreamDisturbed", args)
      ->ToBoolean()
      ->Value();
}

bool ReadableStreamOperations::isLocked(ScriptState* scriptState,
                                        ScriptValue stream) {
  ASSERT(isReadableStream(scriptState, stream));

  v8::Local<v8::Value> args[] = {stream.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStreamLocked",
                                          args)
      ->ToBoolean()
      ->Value();
}

bool ReadableStreamOperations::isReadable(ScriptState* scriptState,
                                          ScriptValue stream) {
  ASSERT(isReadableStream(scriptState, stream));

  v8::Local<v8::Value> args[] = {stream.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState,
                                          "IsReadableStreamReadable", args)
      ->ToBoolean()
      ->Value();
}

bool ReadableStreamOperations::isClosed(ScriptState* scriptState,
                                        ScriptValue stream) {
  ASSERT(isReadableStream(scriptState, stream));

  v8::Local<v8::Value> args[] = {stream.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStreamClosed",
                                          args)
      ->ToBoolean()
      ->Value();
}

bool ReadableStreamOperations::isErrored(ScriptState* scriptState,
                                         ScriptValue stream) {
  ASSERT(isReadableStream(scriptState, stream));

  v8::Local<v8::Value> args[] = {stream.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState,
                                          "IsReadableStreamErrored", args)
      ->ToBoolean()
      ->Value();
}

bool ReadableStreamOperations::isReadableStreamDefaultReader(
    ScriptState* scriptState,
    ScriptValue value) {
  ASSERT(!value.isEmpty());

  if (!value.isObject())
    return false;

  v8::Local<v8::Value> args[] = {value.v8Value()};
  return V8ScriptRunner::callExtraOrCrash(scriptState,
                                          "IsReadableStreamDefaultReader", args)
      ->ToBoolean()
      ->Value();
}

ScriptPromise ReadableStreamOperations::defaultReaderRead(
    ScriptState* scriptState,
    ScriptValue reader) {
  ASSERT(isReadableStreamDefaultReader(scriptState, reader));

  v8::Local<v8::Value> args[] = {reader.v8Value()};
  return ScriptPromise::cast(
      scriptState, V8ScriptRunner::callExtraOrCrash(
                       scriptState, "ReadableStreamDefaultReaderRead", args));
}

void ReadableStreamOperations::tee(ScriptState* scriptState,
                                   ScriptValue stream,
                                   ScriptValue* newStream1,
                                   ScriptValue* newStream2) {
  DCHECK(isReadableStream(scriptState, stream));
  DCHECK(!isLocked(scriptState, stream));

  v8::Local<v8::Value> args[] = {stream.v8Value()};

  ScriptValue result(scriptState, V8ScriptRunner::callExtraOrCrash(
                                      scriptState, "ReadableStreamTee", args));
  DCHECK(result.v8Value()->IsArray());
  v8::Local<v8::Array> branches = result.v8Value().As<v8::Array>();
  DCHECK_EQ(2u, branches->Length());

  *newStream1 = ScriptValue(
      scriptState, branches->Get(scriptState->context(), 0).ToLocalChecked());
  *newStream2 = ScriptValue(
      scriptState, branches->Get(scriptState->context(), 1).ToLocalChecked());

  DCHECK(isReadableStream(scriptState, *newStream1));
  DCHECK(isReadableStream(scriptState, *newStream2));
}

}  // namespace blink
