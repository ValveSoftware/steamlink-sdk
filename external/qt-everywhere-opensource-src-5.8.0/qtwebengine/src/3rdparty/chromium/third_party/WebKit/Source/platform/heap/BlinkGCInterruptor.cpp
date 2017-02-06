// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/BlinkGCInterruptor.h"

#include "platform/heap/BlinkGC.h"
#include "platform/heap/ThreadState.h"

namespace blink {

void BlinkGCInterruptor::onInterrupted()
{
    ThreadState* state = ThreadState::current();
    ASSERT(state);
    ASSERT(!state->isAtSafePoint());
    state->safePoint(BlinkGC::HeapPointersOnStack);
}

} // namespace blink
