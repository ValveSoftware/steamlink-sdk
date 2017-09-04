// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/ConditionalFeaturesForModules.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8DedicatedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8SharedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/V8WorkerNavigator.h"
#include "bindings/modules/v8/V8DedicatedWorkerGlobalScopePartial.h"
#include "bindings/modules/v8/V8Gamepad.h"
#include "bindings/modules/v8/V8GamepadButton.h"
#include "bindings/modules/v8/V8InstallEvent.h"
#include "bindings/modules/v8/V8NavigatorPartial.h"
#include "bindings/modules/v8/V8ServiceWorkerGlobalScope.h"
#include "bindings/modules/v8/V8SharedWorkerGlobalScopePartial.h"
#include "bindings/modules/v8/V8WindowPartial.h"
#include "bindings/modules/v8/V8WorkerNavigatorPartial.h"
#include "core/dom/ExecutionContext.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction
    s_originalInstallConditionalFeaturesFunction = nullptr;
}

void installConditionalFeaturesForModules(
    const WrapperTypeInfo* wrapperTypeInfo,
    const ScriptState* scriptState,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject) {
  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attibutes and interfaces in IDL
  // files. (crbug.com/615060)
  (*s_originalInstallConditionalFeaturesFunction)(
      wrapperTypeInfo, scriptState, prototypeObject, interfaceObject);

  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (!executionContext)
    return;
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  v8::Local<v8::Object> global = scriptState->context()->Global();
  if (wrapperTypeInfo == &V8Navigator::wrapperTypeInfo) {
    if (OriginTrials::webShareEnabled(executionContext)) {
      V8NavigatorPartial::installWebShare(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
    if (OriginTrials::webUSBEnabled(executionContext)) {
      V8NavigatorPartial::installWebUSB(isolate, world, v8::Local<v8::Object>(),
                                        prototypeObject, interfaceObject);
    }
    if (OriginTrials::webVREnabled(executionContext)) {
      V8NavigatorPartial::installWebVR(isolate, world, global, prototypeObject,
                                       interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Window::wrapperTypeInfo) {
    if (OriginTrials::imageCaptureEnabled(executionContext)) {
      V8WindowPartial::installImageCapture(isolate, world, global,
                                           prototypeObject, interfaceObject);
    }
    if (OriginTrials::webUSBEnabled(executionContext)) {
      V8WindowPartial::installWebUSB(isolate, world, global, prototypeObject,
                                     interfaceObject);
    }
    if (OriginTrials::webVREnabled(executionContext)) {
      V8WindowPartial::installWebVR(isolate, world, global, prototypeObject,
                                    interfaceObject);
    }
    if (OriginTrials::gamepadExtensionsEnabled(executionContext)) {
      V8WindowPartial::installGamepadExtensions(
          isolate, world, global, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8ServiceWorkerGlobalScope::wrapperTypeInfo) {
    if (OriginTrials::foreignFetchEnabled(executionContext)) {
      V8ServiceWorkerGlobalScope::installForeignFetch(
          isolate, world, global, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8InstallEvent::wrapperTypeInfo) {
    if (OriginTrials::foreignFetchEnabled(executionContext)) {
      V8InstallEvent::installForeignFetch(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Gamepad::wrapperTypeInfo) {
    if (OriginTrials::gamepadExtensionsEnabled(executionContext)) {
      V8Gamepad::installGamepadExtensions(isolate, world, global,
                                          prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8GamepadButton::wrapperTypeInfo) {
    if (OriginTrials::gamepadExtensionsEnabled(executionContext)) {
      V8GamepadButton::installGamepadExtensions(
          isolate, world, global, prototypeObject, interfaceObject);
    }
  }
}

void registerInstallConditionalFeaturesForModules() {
  s_originalInstallConditionalFeaturesFunction =
      setInstallConditionalFeaturesFunction(
          &installConditionalFeaturesForModules);
}

}  // namespace blink
