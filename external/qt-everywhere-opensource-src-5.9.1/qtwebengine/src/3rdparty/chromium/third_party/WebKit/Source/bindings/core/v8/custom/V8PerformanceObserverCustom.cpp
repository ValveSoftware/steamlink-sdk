// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PerformanceObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Performance.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/PerformanceObserver.h"

namespace blink {

void V8PerformanceObserver::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::throwTypeError(
        info.GetIsolate(),
        ExceptionMessages::failedToConstruct(
            "PerformanceObserver",
            ExceptionMessages::notEnoughArguments(1, info.Length())));
    return;
  }

  v8::Local<v8::Object> wrapper = info.Holder();

  Performance* performance = nullptr;
  DOMWindow* window = toDOMWindow(wrapper->CreationContext());
  if (!window) {
    V8ThrowException::throwTypeError(
        info.GetIsolate(),
        ExceptionMessages::failedToConstruct(
            "PerformanceObserver", "No 'window' in current context."));
    return;
  }
  performance = DOMWindowPerformance::performance(*window);
  ASSERT(performance);

  if (info.Length() <= 0 || !info[0]->IsFunction()) {
    V8ThrowException::throwTypeError(
        info.GetIsolate(),
        ExceptionMessages::failedToConstruct(
            "PerformanceObserver",
            "The callback provided as parameter 1 is not a function."));
    return;
  }
  ScriptState* scriptState = ScriptState::forReceiverObject(info);
  v8::Local<v8::Function> v8Callback = v8::Local<v8::Function>::Cast(info[0]);
  PerformanceObserverCallback* callback =
      PerformanceObserverCallback::create(scriptState, v8Callback);

  PerformanceObserver* observer =
      PerformanceObserver::create(scriptState, performance, callback);

  // TODO(bashi): Don't set private property (and remove this custom
  // constructor) when we can call setWrapperReference() correctly.
  // crbug.com/468240.
  V8PrivateProperty::getPerformanceObserverCallback(info.GetIsolate())
      .set(info.GetIsolate()->GetCurrentContext(), wrapper, v8Callback);
  v8SetReturnValue(info,
                   V8DOMWrapper::associateObjectWithWrapper(
                       info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

}  // namespace blink
