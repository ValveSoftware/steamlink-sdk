/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/ActiveDOMObject.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/InstanceCounters.h"

namespace blink {

ActiveDOMObject::ActiveDOMObject(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext, ActiveDOMObjectType)
#if DCHECK_IS_ON()
    , m_suspendIfNeededCalled(false)
#endif
{
    DCHECK(!executionContext || executionContext->isContextThread());
    // TODO(hajimehoshi): Now the leak detector can't treat vaious threads other
    // than the main thread and worker threads. After fixing the leak detector,
    // let's count objects on other threads as many as possible.
    if (isMainThread())
        InstanceCounters::incrementCounter(InstanceCounters::ActiveDOMObjectCounter);
}

ActiveDOMObject::~ActiveDOMObject()
{
    if (isMainThread())
        InstanceCounters::decrementCounter(InstanceCounters::ActiveDOMObjectCounter);

#if DCHECK_IS_ON()
    DCHECK(m_suspendIfNeededCalled);
#endif
}

void ActiveDOMObject::suspendIfNeeded()
{
#if DCHECK_IS_ON()
    DCHECK(!m_suspendIfNeededCalled);
    m_suspendIfNeededCalled = true;
#endif
    if (ExecutionContext* context = getExecutionContext())
        context->suspendActiveDOMObjectIfNeeded(this);
}

void ActiveDOMObject::suspend()
{
}

void ActiveDOMObject::resume()
{
}

void ActiveDOMObject::stop()
{
}

void ActiveDOMObject::didMoveToNewExecutionContext(ExecutionContext* context)
{
    setContext(context);

    if (context->activeDOMObjectsAreStopped()) {
        stop();
        return;
    }

    if (context->activeDOMObjectsAreSuspended()) {
        suspend();
        return;
    }

    resume();
}

} // namespace blink
