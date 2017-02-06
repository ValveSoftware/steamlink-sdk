// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/Visitor.h"

#include "platform/heap/BlinkGC.h"
#include "platform/heap/MarkingVisitor.h"
#include "platform/heap/ThreadState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<Visitor> Visitor::create(ThreadState* state, BlinkGC::GCType gcType)
{
    switch (gcType) {
    case BlinkGC::GCWithSweep:
    case BlinkGC::GCWithoutSweep:
        return wrapUnique(new MarkingVisitor<Visitor::GlobalMarking>(state));
    case BlinkGC::TakeSnapshot:
        return wrapUnique(new MarkingVisitor<Visitor::SnapshotMarking>(state));
    case BlinkGC::ThreadTerminationGC:
        return wrapUnique(new MarkingVisitor<Visitor::ThreadLocalMarking>(state));
    case BlinkGC::ThreadLocalWeakProcessing:
        return wrapUnique(new MarkingVisitor<Visitor::WeakProcessing>(state));
    default:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

Visitor::Visitor(ThreadState* state, MarkingMode markingMode)
    : VisitorHelper(state)
    , m_markingMode(markingMode)
{
    // See ThreadState::runScheduledGC() why we need to already be in a
    // GCForbiddenScope before any safe point is entered.
    state->enterGCForbiddenScope();

    ASSERT(state->checkThread());
}

Visitor::~Visitor()
{
    state()->leaveGCForbiddenScope();
}

} // namespace blink
