// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8IDBObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/modules/v8/IDBObserverCallback.h"

namespace blink {

void V8IDBObserver::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(
      info.GetIsolate(), ExceptionState::ConstructionContext, "IDBObserver");

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.throwTypeError(
        ExceptionMessages::notEnoughArguments(1, info.Length()));
    return;
  }

  v8::Local<v8::Object> wrapper = info.Holder();

  if (!info[0]->IsFunction()) {
    exceptionState.throwTypeError(
        "The callback provided as parameter 1 is not a function.");
    return;
  }

  if (info.Length() > 1 && !isUndefinedOrNull(info[1]) &&
      !info[1]->IsObject()) {
    exceptionState.throwTypeError("parameter 2 ('options') is not an object.");
    return;
  }

  if (exceptionState.hadException())
    return;

  ScriptState* scriptState = ScriptState::forReceiverObject(info);
  v8::Local<v8::Function> v8Callback = v8::Local<v8::Function>::Cast(info[0]);
  IDBObserverCallback* callback =
      IDBObserverCallback::create(scriptState, v8Callback);
  IDBObserver* observer = IDBObserver::create(callback);
  if (exceptionState.hadException())
    return;
  DCHECK(observer);
  // TODO(bashi): Don't set private property (and remove this custom
  // constructor) when we can call setWrapperReference() correctly.
  V8PrivateProperty::getIDBObserverCallback(info.GetIsolate())
      .set(info.GetIsolate()->GetCurrentContext(), wrapper, v8Callback);
  v8SetReturnValue(info,
                   V8DOMWrapper::associateObjectWithWrapper(
                       info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

}  // namespace blink
