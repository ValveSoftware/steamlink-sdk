// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/testing/InternalsFrobulate.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

// static
bool InternalsFrobulate::frobulate(ScriptState* scriptState, Internals& internals, ExceptionState& exceptionState)
{
    String errorMessage;
    if (!OriginTrials::originTrialsSampleAPIEnabled(scriptState->getExecutionContext(), errorMessage)) {
        exceptionState.throwDOMException(NotSupportedError, errorMessage);
        return false;
    }
    return frobulateNoEnabledCheck(internals);
}

// static
bool InternalsFrobulate::frobulateNoEnabledCheck(Internals& internals)
{
    return true;
}

} // namespace blink
