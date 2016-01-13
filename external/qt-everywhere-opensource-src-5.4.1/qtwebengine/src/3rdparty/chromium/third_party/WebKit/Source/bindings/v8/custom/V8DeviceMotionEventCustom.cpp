/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "bindings/modules/v8/V8DeviceMotionEvent.h"

#include "bindings/v8/V8Binding.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include <v8.h>

namespace WebCore {

namespace {

PassRefPtrWillBeRawPtr<DeviceMotionData::Acceleration> readAccelerationArgument(v8::Local<v8::Value> value, v8::Isolate* isolate)
{
    if (isUndefinedOrNull(value))
        return nullptr;

    // Given the test above, this will always yield an object.
    v8::Local<v8::Object> object = value->ToObject();

    v8::Local<v8::Value> xValue = object->Get(v8AtomicString(isolate, "x"));
    if (xValue.IsEmpty())
        return nullptr;
    bool canProvideX = !isUndefinedOrNull(xValue);
    double x = xValue->NumberValue();

    v8::Local<v8::Value> yValue = object->Get(v8AtomicString(isolate, "y"));
    if (yValue.IsEmpty())
        return nullptr;
    bool canProvideY = !isUndefinedOrNull(yValue);
    double y = yValue->NumberValue();

    v8::Local<v8::Value> zValue = object->Get(v8AtomicString(isolate, "z"));
    if (zValue.IsEmpty())
        return nullptr;
    bool canProvideZ = !isUndefinedOrNull(zValue);
    double z = zValue->NumberValue();

    if (!canProvideX && !canProvideY && !canProvideZ)
        return nullptr;

    return DeviceMotionData::Acceleration::create(canProvideX, x, canProvideY, y, canProvideZ, z);
}

PassRefPtrWillBeRawPtr<DeviceMotionData::RotationRate> readRotationRateArgument(v8::Local<v8::Value> value, v8::Isolate* isolate)
{
    if (isUndefinedOrNull(value))
        return nullptr;

    // Given the test above, this will always yield an object.
    v8::Local<v8::Object> object = value->ToObject();

    v8::Local<v8::Value> alphaValue = object->Get(v8AtomicString(isolate, "alpha"));
    if (alphaValue.IsEmpty())
        return nullptr;
    bool canProvideAlpha = !isUndefinedOrNull(alphaValue);
    double alpha = alphaValue->NumberValue();

    v8::Local<v8::Value> betaValue = object->Get(v8AtomicString(isolate, "beta"));
    if (betaValue.IsEmpty())
        return nullptr;
    bool canProvideBeta = !isUndefinedOrNull(betaValue);
    double beta = betaValue->NumberValue();

    v8::Local<v8::Value> gammaValue = object->Get(v8AtomicString(isolate, "gamma"));
    if (gammaValue.IsEmpty())
        return nullptr;
    bool canProvideGamma = !isUndefinedOrNull(gammaValue);
    double gamma = gammaValue->NumberValue();

    if (!canProvideAlpha && !canProvideBeta && !canProvideGamma)
        return nullptr;

    return DeviceMotionData::RotationRate::create(canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);
}

} // namespace

void V8DeviceMotionEvent::initDeviceMotionEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DeviceMotionEvent* impl = V8DeviceMotionEvent::toNative(info.Holder());
    v8::Isolate* isolate = info.GetIsolate();
    TOSTRING_VOID(V8StringResource<>, type, info[0]);
    bool bubbles = info[1]->BooleanValue();
    bool cancelable = info[2]->BooleanValue();
    RefPtrWillBeRawPtr<DeviceMotionData::Acceleration> acceleration = readAccelerationArgument(info[3], isolate);
    RefPtrWillBeRawPtr<DeviceMotionData::Acceleration> accelerationIncludingGravity = readAccelerationArgument(info[4], isolate);
    RefPtrWillBeRawPtr<DeviceMotionData::RotationRate> rotationRate = readRotationRateArgument(info[5], isolate);
    bool intervalProvided = !isUndefinedOrNull(info[6]);
    double interval = info[6]->NumberValue();
    RefPtrWillBeRawPtr<DeviceMotionData> deviceMotionData = DeviceMotionData::create(acceleration.release(), accelerationIncludingGravity.release(), rotationRate.release(), intervalProvided, interval);
    impl->initDeviceMotionEvent(type, bubbles, cancelable, deviceMotionData.get());
}

} // namespace WebCore
