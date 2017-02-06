// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IteratorResultValue.h"

namespace blink {

v8::Local<v8::Object> v8IteratorResultValue(v8::Isolate* isolate, bool done, v8::Local<v8::Value> value)
{
    v8::Local<v8::Object> result = v8::Object::New(isolate);
    if (value.IsEmpty())
        value = v8::Undefined(isolate);
    if (!v8CallBoolean(result->CreateDataProperty(isolate->GetCurrentContext(), v8String(isolate, "done"), v8Boolean(done, isolate)))
        || !v8CallBoolean(result->CreateDataProperty(isolate->GetCurrentContext(), v8String(isolate, "value"), value)))
        return v8::Local<v8::Object>();
    return result;
}

v8::MaybeLocal<v8::Value> v8UnpackIteratorResult(ScriptState* scriptState, v8::Local<v8::Object> result, bool* done)
{
    v8::MaybeLocal<v8::Value> maybeValue = result->Get(scriptState->context(), v8String(scriptState->isolate(), "value"));
    if (maybeValue.IsEmpty())
        return maybeValue;
    v8::Local<v8::Value> doneValue;
    if (!v8Call(result->Get(scriptState->context(), v8String(scriptState->isolate(), "done")), doneValue)
        || !v8Call(doneValue->BooleanValue(scriptState->context()), *done)) {
        return v8::MaybeLocal<v8::Value>();
    }
    return maybeValue;
}

} // namespace blink
