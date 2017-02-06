// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutator_h
#define CompositorMutator_h

#include "platform/heap/Handle.h"

namespace blink {

class CompositorMutableStateProvider;

class CompositorMutator : public GarbageCollectedFinalized<CompositorMutator> {
public:
    virtual ~CompositorMutator() {}

    DEFINE_INLINE_VIRTUAL_TRACE() {}

    // Called from compositor thread to run the animation frame callbacks from all
    // connected CompositorWorkers.
    // Returns true if any animation callbacks requested an animation frame
    // (i.e. should be reinvoked next frame).
    virtual bool mutate(double monotonicTimeNow, CompositorMutableStateProvider*) = 0;
};

} // namespace blink

#endif // CompositorMutator_h
