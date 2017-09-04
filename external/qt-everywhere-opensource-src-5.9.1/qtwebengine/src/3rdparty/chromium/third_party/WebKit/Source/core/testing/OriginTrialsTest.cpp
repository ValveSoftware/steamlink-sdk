// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OriginTrialsTest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

bool OriginTrialsTest::normalAttribute() {
  return true;
}

// static
bool OriginTrialsTest::staticAttribute() {
  return true;
}

bool OriginTrialsTest::throwingAttribute(ScriptState* scriptState,
                                         ExceptionState& exceptionState) {
  String errorMessage;
  if (!OriginTrials::originTrialsSampleAPIEnabled(
          scriptState->getExecutionContext())) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "The Origin Trials Sample API has not been enabled in this context");
    return false;
  }
  return unconditionalAttribute();
}

bool OriginTrialsTest::unconditionalAttribute() {
  return true;
}

DEFINE_TRACE(OriginTrialsTest) {}

}  // namespace blink
