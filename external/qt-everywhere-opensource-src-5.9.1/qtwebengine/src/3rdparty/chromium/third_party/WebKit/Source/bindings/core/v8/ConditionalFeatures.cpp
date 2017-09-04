// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ConditionalFeatures.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8HTMLLinkElement.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/origin_trials/OriginTrialContext.h"

namespace blink {

void installConditionalFeaturesCore(const WrapperTypeInfo* wrapperTypeInfo,
                                    const ScriptState* scriptState,
                                    v8::Local<v8::Object> prototypeObject,
                                    v8::Local<v8::Function> interfaceObject) {
  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (!executionContext)
    return;
  OriginTrialContext* originTrialContext = OriginTrialContext::from(
      executionContext, OriginTrialContext::DontCreateIfNotExists);
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  if (wrapperTypeInfo == &V8HTMLLinkElement::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::linkServiceWorkerEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("ForeignFetch"))) {
      V8HTMLLinkElement::installLinkServiceWorker(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Window::wrapperTypeInfo) {
    v8::Local<v8::Object> instanceObject = scriptState->context()->Global();
    if (RuntimeEnabledFeatures::longTaskObserverEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("LongTaskObserver"))) {
      V8Window::installLongTaskObserver(isolate, world, instanceObject,
                                        prototypeObject, interfaceObject);
    }
  }
}

namespace {

InstallConditionalFeaturesFunction s_installConditionalFeaturesFunction =
    &installConditionalFeaturesCore;
}

InstallConditionalFeaturesFunction setInstallConditionalFeaturesFunction(
    InstallConditionalFeaturesFunction newInstallConditionalFeaturesFunction) {
  InstallConditionalFeaturesFunction originalFunction =
      s_installConditionalFeaturesFunction;
  s_installConditionalFeaturesFunction = newInstallConditionalFeaturesFunction;
  return originalFunction;
}

void installConditionalFeatures(const WrapperTypeInfo* type,
                                const ScriptState* scriptState,
                                v8::Local<v8::Object> prototypeObject,
                                v8::Local<v8::Function> interfaceObject) {
  (*s_installConditionalFeaturesFunction)(type, scriptState, prototypeObject,
                                          interfaceObject);
}

void installPendingConditionalFeaturesOnWindow(const ScriptState* scriptState) {
  DCHECK(scriptState);
  DCHECK(scriptState->context() == scriptState->isolate()->GetCurrentContext());
  DCHECK(scriptState->perContextData());
  DCHECK(scriptState->world().isMainWorld());
  (*s_installConditionalFeaturesFunction)(&V8Window::wrapperTypeInfo,
                                          scriptState, v8::Local<v8::Object>(),
                                          v8::Local<v8::Function>());
}

bool isFeatureEnabledInFrame(const FeaturePolicy::Feature& feature,
                             const LocalFrame* frame) {
  // If there is no frame, or if feature policy is disabled, use defaults.
  bool enabledByDefault =
      (feature.defaultPolicy != FeaturePolicy::FeatureDefault::DisableForAll);
  if (!RuntimeEnabledFeatures::featurePolicyEnabled() || !frame)
    return enabledByDefault;
  FeaturePolicy* featurePolicy = frame->securityContext()->getFeaturePolicy();
  if (!featurePolicy)
    return enabledByDefault;

  // Otherwise, check policy.
  return featurePolicy->isFeatureEnabled(feature);
}

}  // namespace blink
