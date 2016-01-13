/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/modules/v8/V8Geolocation.h"

#include "bindings/modules/v8/V8PositionCallback.h"
#include "bindings/modules/v8/V8PositionErrorCallback.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8Callback.h"
#include "modules/geolocation/Geolocation.h"

using namespace WTF;

namespace WebCore {

static PositionOptions* createPositionOptions(v8::Local<v8::Value> value, v8::Isolate* isolate, bool& succeeded, ExceptionState& exceptionState)
{
    succeeded = true;

    // Create default options.
    PositionOptions* options = PositionOptions::create();

    // Argument is optional (hence undefined is allowed), and null is allowed.
    if (isUndefinedOrNull(value)) {
        // Use default options.
        return options;
    }

    // Given the above test, this will always yield an object.
    v8::Local<v8::Object> object = value->ToObject();

    // For all three properties, we apply the following ...
    // - If the getter or the property's valueOf method throws an exception, we
    //   quit so as not to risk overwriting the exception.
    // - If the value is absent or undefined, we don't override the default.
    v8::Local<v8::Value> enableHighAccuracyValue = object->Get(v8AtomicString(isolate, "enableHighAccuracy"));
    if (enableHighAccuracyValue.IsEmpty()) {
        succeeded = false;
        return nullptr;
    }
    if (!enableHighAccuracyValue->IsUndefined()) {
        v8::Local<v8::Boolean> enableHighAccuracyBoolean = enableHighAccuracyValue->ToBoolean();
        if (enableHighAccuracyBoolean.IsEmpty()) {
            succeeded = false;
            return nullptr;
        }
        options->setEnableHighAccuracy(enableHighAccuracyBoolean->Value());
    }

    v8::Local<v8::Value> timeoutValue = object->Get(v8AtomicString(isolate, "timeout"));
    if (timeoutValue.IsEmpty()) {
        succeeded = false;
        return nullptr;
    }
    if (!timeoutValue->IsUndefined()) {
        v8::Local<v8::Number> timeoutNumber = timeoutValue->ToNumber();
        if (timeoutNumber.IsEmpty()) {
            succeeded = false;
            return nullptr;
        }
        if (timeoutNumber->Value() <= 0)
            options->setTimeout(0);
        else
            options->setTimeout(toUInt32(timeoutValue, Clamp, exceptionState));
    }

    v8::Local<v8::Value> maximumAgeValue = object->Get(v8AtomicString(isolate, "maximumAge"));
    if (maximumAgeValue.IsEmpty()) {
        succeeded = false;
        return nullptr;
    }
    if (!maximumAgeValue->IsUndefined()) {
        v8::Local<v8::Number> maximumAgeNumber = maximumAgeValue->ToNumber();
        if (maximumAgeNumber.IsEmpty()) {
            succeeded = false;
            return nullptr;
        }
        if (maximumAgeNumber->Value() <= 0)
            options->setMaximumAge(0);
        else
            options->setMaximumAge(toUInt32(maximumAgeValue, Clamp, exceptionState));
    }

    return options;
}

void V8Geolocation::getCurrentPositionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    bool succeeded = false;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "getCurrentPosition", "Geolocation", info.Holder(), info.GetIsolate());

    OwnPtr<PositionCallback> positionCallback = createFunctionOnlyCallback<V8PositionCallback>(info[0], 1, succeeded, info.GetIsolate(), exceptionState);
    if (!succeeded)
        return;
    ASSERT(positionCallback);

    // Argument is optional (hence undefined is allowed), and null is allowed.
    OwnPtr<PositionErrorCallback> positionErrorCallback = createFunctionOnlyCallback<V8PositionErrorCallback>(info[1], 2, succeeded, info.GetIsolate(), exceptionState, CallbackAllowUndefined | CallbackAllowNull);
    if (!succeeded)
        return;

    PositionOptions* positionOptions = createPositionOptions(info[2], info.GetIsolate(), succeeded, exceptionState);
    if (!succeeded)
        return;
    ASSERT(positionOptions);

    Geolocation* geolocation = V8Geolocation::toNative(info.Holder());
    geolocation->getCurrentPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions);
}

void V8Geolocation::watchPositionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    bool succeeded = false;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "watchCurrentPosition", "Geolocation", info.Holder(), info.GetIsolate());

    OwnPtr<PositionCallback> positionCallback = createFunctionOnlyCallback<V8PositionCallback>(info[0], 1, succeeded, info.GetIsolate(), exceptionState);
    if (!succeeded)
        return;
    ASSERT(positionCallback);

    // Argument is optional (hence undefined is allowed), and null is allowed.
    OwnPtr<PositionErrorCallback> positionErrorCallback = createFunctionOnlyCallback<V8PositionErrorCallback>(info[1], 2, succeeded, info.GetIsolate(), exceptionState, CallbackAllowUndefined | CallbackAllowNull);
    if (!succeeded)
        return;

    PositionOptions* positionOptions = createPositionOptions(info[2], info.GetIsolate(), succeeded, exceptionState);
    if (!succeeded)
        return;
    ASSERT(positionOptions);

    Geolocation* geolocation = V8Geolocation::toNative(info.Holder());
    int watchId = geolocation->watchPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions);
    v8SetReturnValue(info, watchId);
}

} // namespace WebCore
