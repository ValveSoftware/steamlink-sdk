// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_params.h"

namespace metrics {

CallStackProfileParams::CallStackProfileParams()
    : CallStackProfileParams(UNKNOWN_PROCESS, UNKNOWN_THREAD, UNKNOWN) {}

CallStackProfileParams::CallStackProfileParams(Process process, Thread thread,
                                               Trigger trigger)
    : CallStackProfileParams(process, thread, trigger, MAY_SHUFFLE) {}

CallStackProfileParams::CallStackProfileParams(Process process, Thread thread,
                                               Trigger trigger,
                                               SampleOrderingSpec ordering_spec)
    : process(process), thread(thread), trigger(trigger),
      ordering_spec(ordering_spec) {}

}  // namespace metrics
