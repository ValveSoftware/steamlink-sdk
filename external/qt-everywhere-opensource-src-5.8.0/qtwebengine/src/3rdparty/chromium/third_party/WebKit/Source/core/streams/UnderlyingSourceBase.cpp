// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/UnderlyingSourceBase.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/streams/ReadableStreamController.h"
#include <v8.h>

namespace blink {

ScriptPromise UnderlyingSourceBase::startWrapper(ScriptState* scriptState, ScriptValue jsController)
{
    // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to construct multiple streams)
    ASSERT(!m_controller);

    m_controller = new ReadableStreamController(jsController);

    return start(scriptState);
}

ScriptPromise UnderlyingSourceBase::start(ScriptState* scriptState)
{
    return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise UnderlyingSourceBase::pull(ScriptState* scriptState)
{
    return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise UnderlyingSourceBase::cancelWrapper(ScriptState* scriptState, ScriptValue reason)
{
    m_controller->noteHasBeenCanceled();
    return cancel(scriptState, reason);
}

ScriptPromise UnderlyingSourceBase::cancel(ScriptState* scriptState, ScriptValue reason)
{
    return ScriptPromise::castUndefined(scriptState);
}

void UnderlyingSourceBase::notifyLockAcquired()
{
    m_isStreamLocked = true;
}

void UnderlyingSourceBase::notifyLockReleased()
{
    m_isStreamLocked = false;
}

bool UnderlyingSourceBase::hasPendingActivity() const
{
    // This will return false within a finite time period _assuming_ that
    // consumers use the controller to close or error the stream.
    // Browser-created readable streams should always close or error within a
    // finite time period, due to timeouts etc.
    return m_controller && m_controller->isActive() && m_isStreamLocked;
}

void UnderlyingSourceBase::stop()
{
    if (m_controller) {
        m_controller->noteHasBeenCanceled();
        m_controller.clear();
    }
}

DEFINE_TRACE(UnderlyingSourceBase)
{
    ActiveDOMObject::trace(visitor);
    visitor->trace(m_controller);
}

} // namespace blink
