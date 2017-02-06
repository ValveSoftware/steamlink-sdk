// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IteratorResultValue_h
#define V8IteratorResultValue_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8.h"
#include "core/CoreExport.h"
#include <v8.h>

namespace blink {

// "Iterator result" in this file is an object returned from iterator.next()
// having two members "done" and "value".

CORE_EXPORT v8::Local<v8::Object> v8IteratorResultValue(v8::Isolate*, bool done, v8::Local<v8::Value>);

// Unpacks |result|, stores the value of "done" member to |done| and returns
// the value of "value" member. Returns an empty handle when errored.
CORE_EXPORT v8::MaybeLocal<v8::Value> v8UnpackIteratorResult(ScriptState*, v8::Local<v8::Object> result, bool* done);

template<typename T>
inline ScriptValue v8IteratorResult(ScriptState* scriptState, const T& value)
{
    return ScriptValue(
        scriptState,
        v8IteratorResultValue(
            scriptState->isolate(),
            false,
            toV8(value, scriptState->context()->Global(), scriptState->isolate())));
}

inline ScriptValue v8IteratorResultDone(ScriptState* scriptState)
{
    return ScriptValue(
        scriptState,
        v8IteratorResultValue(
            scriptState->isolate(),
            true,
            v8::Undefined(scriptState->isolate())));
}

} // namespace blink

#endif // V8IteratorResultValue_h
