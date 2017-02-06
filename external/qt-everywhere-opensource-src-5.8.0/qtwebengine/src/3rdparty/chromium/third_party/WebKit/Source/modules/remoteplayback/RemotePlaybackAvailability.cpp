// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlaybackAvailability.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "modules/EventTargetModulesNames.h"

namespace blink {

// static
RemotePlaybackAvailability* RemotePlaybackAvailability::take(ScriptPromiseResolver* resolver, bool value)
{
    return new RemotePlaybackAvailability(resolver->getExecutionContext(), value);
}

RemotePlaybackAvailability::RemotePlaybackAvailability(ExecutionContext* executionContext, bool value)
    : ActiveScriptWrappable(this)
    , ContextLifecycleObserver(executionContext)
    , m_value(value)
{
    ASSERT(executionContext->isDocument());
}

RemotePlaybackAvailability::~RemotePlaybackAvailability() = default;

const AtomicString& RemotePlaybackAvailability::interfaceName() const
{
    return EventTargetNames::RemotePlaybackAvailability;
}

ExecutionContext* RemotePlaybackAvailability::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

void RemotePlaybackAvailability::availabilityChanged(bool value)
{
    if (m_value == value)
        return;

    m_value = value;
    dispatchEvent(Event::create(EventTypeNames::change));
}

bool RemotePlaybackAvailability::value() const
{
    return m_value;
}

bool RemotePlaybackAvailability::hasPendingActivity() const
{
    return hasEventListeners();
}

DEFINE_TRACE(RemotePlaybackAvailability)
{
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink
