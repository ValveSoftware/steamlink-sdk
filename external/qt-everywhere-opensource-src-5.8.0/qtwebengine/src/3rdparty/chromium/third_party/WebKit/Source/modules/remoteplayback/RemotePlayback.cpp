// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/EventTargetModules.h"
#include "modules/remoteplayback/RemotePlaybackAvailability.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

namespace {

const AtomicString& remotePlaybackStateToString(WebRemotePlaybackState state)
{
    DEFINE_STATIC_LOCAL(const AtomicString, connectedValue, ("connected"));
    DEFINE_STATIC_LOCAL(const AtomicString, disconnectedValue, ("disconnected"));

    switch (state) {
    case WebRemotePlaybackState::Connected:
        return connectedValue;
    case WebRemotePlaybackState::Disconnected:
        return disconnectedValue;
    }

    ASSERT_NOT_REACHED();
    return disconnectedValue;
}

} // anonymous namespace

// static
RemotePlayback* RemotePlayback::create(HTMLMediaElement& element)
{
    ASSERT(element.document().frame());

    RemotePlayback* remotePlayback = new RemotePlayback(element);
    element.setRemotePlaybackClient(remotePlayback);

    return remotePlayback;
}

RemotePlayback::RemotePlayback(HTMLMediaElement& element)
    : ActiveScriptWrappable(this)
    , m_state(element.isPlayingRemotely() ? WebRemotePlaybackState::Connected : WebRemotePlaybackState::Disconnected)
    , m_availability(element.hasRemoteRoutes())
    , m_mediaElement(&element)
{
}

const AtomicString& RemotePlayback::interfaceName() const
{
    return EventTargetNames::RemotePlayback;
}

ExecutionContext* RemotePlayback::getExecutionContext() const
{
    return &m_mediaElement->document();
}

ScriptPromise RemotePlayback::getAvailability(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // TODO(avayvod): currently the availability is tracked for each media element
    // as soon as it's created, we probably want to limit that to when the page/element
    // is visible (see https://crbug.com/597281) and has default controls. If there's
    // no default controls, we should also start tracking availability on demand
    // meaning the Promise returned by getAvailability() will be resolved asynchronously.
    RemotePlaybackAvailability* availability = RemotePlaybackAvailability::take(resolver, m_availability);
    m_availabilityObjects.append(availability);
    resolver->resolve(availability);
    return promise;
}

ScriptPromise RemotePlayback::connect(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // TODO(avayvod): should we have a separate flag to disable the user gesture
    // requirement (for tests) or reuse the one for the PresentationRequest::start()?
    if (!UserGestureIndicator::utilizeUserGesture()) {
        resolver->reject(DOMException::create(InvalidAccessError, "RemotePlayback::connect() requires user gesture."));
        return promise;
    }

    if (m_state == WebRemotePlaybackState::Disconnected) {
        m_connectPromiseResolvers.append(resolver);
        m_mediaElement->requestRemotePlayback();
    } else {
        m_mediaElement->requestRemotePlaybackControl();
        resolver->resolve(false);
    }

    return promise;
}

String RemotePlayback::state() const
{
    return remotePlaybackStateToString(m_state);
}

bool RemotePlayback::hasPendingActivity() const
{
    return hasEventListeners()
        || !m_availabilityObjects.isEmpty()
        || !m_connectPromiseResolvers.isEmpty();
}

void RemotePlayback::stateChanged(WebRemotePlaybackState state)
{
    // We may get a "disconnected" state change while in the "disconnected"
    // state if initiated connection fails. So cleanup the promise resolvers
    // before checking if anything changed.
    // TODO(avayvod): cleanup this logic when we implementing the "connecting"
    // state.
    if (state != WebRemotePlaybackState::Disconnected) {
        for (auto& resolver : m_connectPromiseResolvers)
            resolver->resolve(true);
    } else {
        for (auto& resolver : m_connectPromiseResolvers)
            resolver->reject(DOMException::create(AbortError, "Failed to connect to the remote device."));
    }
    m_connectPromiseResolvers.clear();

    if (m_state == state)
        return;

    m_state = state;
    dispatchEvent(Event::create(EventTypeNames::statechange));
}

void RemotePlayback::availabilityChanged(bool available)
{
    if (m_availability == available)
        return;

    m_availability = available;
    for (auto& availabilityObject : m_availabilityObjects)
        availabilityObject->availabilityChanged(available);
}

void RemotePlayback::connectCancelled()
{
    for (auto& resolver : m_connectPromiseResolvers)
        resolver->resolve(false);
    m_connectPromiseResolvers.clear();
}

DEFINE_TRACE(RemotePlayback)
{
    visitor->trace(m_availabilityObjects);
    visitor->trace(m_connectPromiseResolvers);
    visitor->trace(m_mediaElement);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
