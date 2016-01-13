/*
* Copyright (C) 2012 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef V8Callback_h
#define V8Callback_h

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include <v8.h>

namespace WebCore {

class ExecutionContext;

// Returns false if the callback threw an exception, true otherwise.
bool invokeCallback(ScriptState*, v8::Local<v8::Function> callback, int argc, v8::Handle<v8::Value> argv[]);
bool invokeCallback(ScriptState*, v8::Local<v8::Function> callback, v8::Handle<v8::Value> thisValue, int argc, v8::Handle<v8::Value> argv[]);

// FIXME: This file is used only by V8GeolocationCustom.cpp. Remove the custom binding and this file.
enum CallbackAllowedValueFlag {
    CallbackAllowUndefined = 1,
    CallbackAllowNull = 1 << 1
};

typedef unsigned CallbackAllowedValueFlags;

// 'FunctionOnly' is assumed for the created callback.
template <typename V8CallbackType>
PassOwnPtr<V8CallbackType> createFunctionOnlyCallback(v8::Local<v8::Value> value, unsigned index, bool& succeeded, v8::Isolate* isolate, ExceptionState& exceptionState, CallbackAllowedValueFlags acceptedValues = 0)
{
    succeeded = true;

    if (value->IsUndefined() && (acceptedValues & CallbackAllowUndefined))
        return nullptr;

    if (value->IsNull() && (acceptedValues & CallbackAllowNull))
        return nullptr;

    if (!value->IsFunction()) {
        succeeded = false;
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(index, "Function"));
        exceptionState.throwIfNeeded();
        return nullptr;
    }

    return V8CallbackType::create(v8::Handle<v8::Function>::Cast(value), ScriptState::current(isolate));
}

} // namespace WebCore

#endif // V8Callback_h
