// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/custom/CustomElementMicrotaskStepDispatcher.h"

#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/html/imports/HTMLImportLoader.h"

namespace WebCore {

CustomElementMicrotaskStepDispatcher::CustomElementMicrotaskStepDispatcher()
    : m_syncQueue(CustomElementSyncMicrotaskQueue::create())
    , m_asyncQueue(CustomElementAsyncImportMicrotaskQueue::create())
{
}

void CustomElementMicrotaskStepDispatcher::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep> step)
{
    if (parentLoader)
        parentLoader->microtaskQueue()->enqueue(step);
    else
        m_syncQueue->enqueue(step);
}

void CustomElementMicrotaskStepDispatcher::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskImportStep> step, bool importIsSync)
{
    if (importIsSync)
        enqueue(parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep>(step));
    else
        m_asyncQueue->enqueue(step);
}

void CustomElementMicrotaskStepDispatcher::dispatch()
{
    m_syncQueue->dispatch();
    if (m_syncQueue->isEmpty())
        m_asyncQueue->dispatch();
}

void CustomElementMicrotaskStepDispatcher::trace(Visitor* visitor)
{
    visitor->trace(m_syncQueue);
    visitor->trace(m_asyncQueue);
}

#if !defined(NDEBUG)
void CustomElementMicrotaskStepDispatcher::show(unsigned indent)
{
    fprintf(stderr, "%*sSync:\n", indent, "");
    m_syncQueue->show(indent + 1);
    fprintf(stderr, "%*sAsync:\n", indent, "");
    m_asyncQueue->show(indent + 1);
}
#endif

} // namespace WebCore
