// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8IDBObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/modules/v8/V8IDBObserverCallback.h"
#include "bindings/modules/v8/V8IDBObserverInit.h"

namespace blink {

void V8IDBObserver::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (UNLIKELY(info.Length() < 1)) {
        V8ThrowException::throwException(createMinimumArityTypeErrorForConstructor(info.GetIsolate(), "IDBObserver", 1, info.Length()), info.GetIsolate());
        return;
    }

    v8::Local<v8::Object> wrapper = info.Holder();

    if (!info[0]->IsFunction()) {
        V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToConstruct("IDBObserver", "The callback provided as parameter 1 is not a function."));
        return;
    }

    if (info.Length() > 1 && !isUndefinedOrNull(info[1]) && !info[1]->IsObject()) {
        V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToConstruct("IDBObserver", "IDBObserverInit (parameter 2) is not an object."));
        return;
    }

    IDBObserverInit idbObserverInit;
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "IDBObserver", info.Holder(), info.GetIsolate());
    V8IDBObserverInit::toImpl(info.GetIsolate(), info[1], idbObserverInit, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;

    IDBObserverCallback* callback = new V8IDBObserverCallback(v8::Local<v8::Function>::Cast(info[0]), wrapper, ScriptState::current(info.GetIsolate()));
    IDBObserver* observer = IDBObserver::create(*callback, idbObserverInit);
    if (exceptionState.throwIfNeeded())
        return;
    DCHECK(observer);
    v8SetReturnValue(info, V8DOMWrapper::associateObjectWithWrapper(info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

} // namespace blink
