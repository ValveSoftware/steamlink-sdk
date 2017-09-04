// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConditionalFeaturesForModules_h
#define ConditionalFeaturesForModules_h

#include "bindings/core/v8/ConditionalFeatures.h"

namespace blink {

void installConditionalFeaturesForModules(const WrapperTypeInfo*,
                                          const ScriptState*,
                                          v8::Local<v8::Object>,
                                          v8::Local<v8::Function>);

void registerInstallConditionalFeaturesForModules();

}  // namespace blink

#endif  // ConditionalFeaturesForModules_h
