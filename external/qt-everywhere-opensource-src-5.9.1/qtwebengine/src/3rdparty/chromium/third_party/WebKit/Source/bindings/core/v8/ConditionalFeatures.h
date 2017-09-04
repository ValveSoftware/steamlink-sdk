// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConditionalFeatures_h
#define ConditionalFeatures_h

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "core/CoreExport.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include <v8.h>

namespace blink {

class ScriptState;
class LocalFrame;

using InstallConditionalFeaturesFunction = void (*)(const WrapperTypeInfo*,
                                                    const ScriptState*,
                                                    v8::Local<v8::Object>,
                                                    v8::Local<v8::Function>);

// Installs all of the conditionally enabled V8 bindings for the given type, in
// a specific context. This is called in V8PerContextData, after the constructor
// and prototype for the type have been created. It indirectly calls the
// function set by |setInstallConditionalFeaturesFunction|.
CORE_EXPORT void installConditionalFeatures(const WrapperTypeInfo*,
                                            const ScriptState*,
                                            v8::Local<v8::Object>,
                                            v8::Local<v8::Function>);

// Sets the function to be called by |installConditionalFeatures|. The function
// is initially set to the private |installConditionalFeaturesCore| function,
// but can be overridden by this function. A pointer to the previously set
// function is returned, so that functions can be chained.
CORE_EXPORT InstallConditionalFeaturesFunction
    setInstallConditionalFeaturesFunction(InstallConditionalFeaturesFunction);

// Installs all of the conditionally enabled V8 bindings on the Window object.
// This is called separately from other objects so that attributes and
// interfaces which need to be visible on the global object are installed even
// when the V8 context is reused (i.e., after navigation)
CORE_EXPORT void installPendingConditionalFeaturesOnWindow(const ScriptState*);

CORE_EXPORT bool isFeatureEnabledInFrame(const FeaturePolicy::Feature&,
                                         const LocalFrame*);

}  // namespace blink

#endif  // ConditionalFeatures_h
