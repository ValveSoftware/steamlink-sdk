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

#include "bindings/modules/v8/V8DeviceMotionEvent.h"

#include "bindings/core/v8/V8Binding.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include <v8.h>

namespace blink {

namespace {

DeviceMotionData::Acceleration* readAccelerationArgument(
    v8::Local<v8::Value> value,
    v8::Isolate* isolate) {
  if (isUndefinedOrNull(value))
    return nullptr;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> object;
  if (!value->ToObject(context).ToLocal(&object))
    return nullptr;

  v8::Local<v8::Value> xValue;
  if (!object->Get(context, v8AtomicString(isolate, "x")).ToLocal(&xValue))
    return nullptr;
  bool canProvideX = !isUndefinedOrNull(xValue);
  double x;
  if (!xValue->NumberValue(context).To(&x))
    return nullptr;

  v8::Local<v8::Value> yValue;
  if (!object->Get(context, v8AtomicString(isolate, "y")).ToLocal(&yValue))
    return nullptr;
  bool canProvideY = !isUndefinedOrNull(yValue);
  double y;
  if (!yValue->NumberValue(context).To(&y))
    return nullptr;

  v8::Local<v8::Value> zValue;
  if (!object->Get(context, v8AtomicString(isolate, "z")).ToLocal(&zValue))
    return nullptr;
  bool canProvideZ = !isUndefinedOrNull(zValue);
  double z;
  if (!zValue->NumberValue(context).To(&z))
    return nullptr;

  if (!canProvideX && !canProvideY && !canProvideZ)
    return nullptr;

  return DeviceMotionData::Acceleration::create(canProvideX, x, canProvideY, y,
                                                canProvideZ, z);
}

DeviceMotionData::RotationRate* readRotationRateArgument(
    v8::Local<v8::Value> value,
    v8::Isolate* isolate) {
  if (isUndefinedOrNull(value))
    return nullptr;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> object;
  if (!value->ToObject(context).ToLocal(&object))
    return nullptr;

  v8::Local<v8::Value> alphaValue;
  if (!object->Get(context, v8AtomicString(isolate, "alpha"))
           .ToLocal(&alphaValue))
    return nullptr;
  bool canProvideAlpha = !isUndefinedOrNull(alphaValue);
  double alpha;
  if (!alphaValue->NumberValue(context).To(&alpha))
    return nullptr;

  v8::Local<v8::Value> betaValue;
  if (!object->Get(context, v8AtomicString(isolate, "beta"))
           .ToLocal(&betaValue))
    return nullptr;
  bool canProvideBeta = !isUndefinedOrNull(betaValue);
  double beta;
  if (!betaValue->NumberValue(context).To(&beta))
    return nullptr;

  v8::Local<v8::Value> gammaValue;
  if (!object->Get(context, v8AtomicString(isolate, "gamma"))
           .ToLocal(&gammaValue))
    return nullptr;
  bool canProvideGamma = !isUndefinedOrNull(gammaValue);
  double gamma;
  if (!gammaValue->NumberValue(context).To(&gamma))
    return nullptr;

  if (!canProvideAlpha && !canProvideBeta && !canProvideGamma)
    return nullptr;

  return DeviceMotionData::RotationRate::create(
      canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);
}

}  // namespace

void V8DeviceMotionEvent::initDeviceMotionEventMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  // TODO(haraken): This custom binding should mimic auto-generated code more.
  ExceptionState exceptionState(ExceptionState::ExecutionContext,
                                "initDeviceMotionEvent", "DeviceMotionEvent",
                                info.Holder(), info.GetIsolate());
  DeviceMotionEvent* impl = V8DeviceMotionEvent::toImpl(info.Holder());
  v8::Isolate* isolate = info.GetIsolate();
  V8StringResource<> type(info[0]);
  if (!type.prepare())
    return;
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  bool bubbles = false;
  if (!info[1]->BooleanValue(context).To(&bubbles))
    return;
  bool cancelable = false;
  if (!info[2]->BooleanValue(context).To(&cancelable))
    return;
  DeviceMotionData::Acceleration* acceleration =
      readAccelerationArgument(info[3], isolate);
  DeviceMotionData::Acceleration* accelerationIncludingGravity =
      readAccelerationArgument(info[4], isolate);
  DeviceMotionData::RotationRate* rotationRate =
      readRotationRateArgument(info[5], isolate);
  bool intervalProvided = !isUndefinedOrNull(info[6]);
  double interval = 0;
  if (intervalProvided) {
    interval = toRestrictedDouble(isolate, info[6], exceptionState);
    if (exceptionState.hadException())
      return;
  }
  DeviceMotionData* deviceMotionData =
      DeviceMotionData::create(acceleration, accelerationIncludingGravity,
                               rotationRate, intervalProvided, interval);
  impl->initDeviceMotionEvent(type, bubbles, cancelable, deviceMotionData);
}

}  // namespace blink
