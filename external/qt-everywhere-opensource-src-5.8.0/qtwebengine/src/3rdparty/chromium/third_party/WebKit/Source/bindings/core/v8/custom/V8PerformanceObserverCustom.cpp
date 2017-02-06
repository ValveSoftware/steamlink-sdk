// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PerformanceObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Performance.h"
#include "bindings/core/v8/V8PerformanceObserverCallback.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/PerformanceObserver.h"

namespace blink {

void V8PerformanceObserver::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (UNLIKELY(info.Length() < 1)) {
        V8ThrowException::throwException(createMinimumArityTypeErrorForMethod(info.GetIsolate(), "createPerformanceObserver", "Performance", 1, info.Length()), info.GetIsolate());
        return;
    }

    v8::Local<v8::Object> wrapper = info.Holder();

    Performance* performance = nullptr;
    DOMWindow* window = toDOMWindow(wrapper->CreationContext());
    if (!window) {
        V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToExecute("createPerformanceObserver", "Performance", "No \"window\" in current context."));
        return;
    }
    performance = DOMWindowPerformance::performance(*window);
    ASSERT(performance);

    PerformanceObserverCallback* callback;
    {
        if (info.Length() <= 0 || !info[0]->IsFunction()) {
            V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToExecute("createPerformanceObserver", "Performance", "The callback provided as parameter 1 is not a function."));
            return;
        }
        callback = V8PerformanceObserverCallback::create(v8::Local<v8::Function>::Cast(info[0]), wrapper, ScriptState::current(info.GetIsolate()));
    }
    PerformanceObserver* observer = PerformanceObserver::create(performance, callback);

    v8SetReturnValue(info, V8DOMWrapper::associateObjectWithWrapper(info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

} // namespace blink
