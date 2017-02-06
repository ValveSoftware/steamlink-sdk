// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/netinfo/NetworkInformation.h"

#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/page/NetworkStateNotifier.h"
#include "modules/EventTargetModules.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/text/WTFString.h"

namespace {

using namespace blink;

String connectionTypeToString(WebConnectionType type)
{
    switch (type) {
    case WebConnectionTypeCellular2G:
    case WebConnectionTypeCellular3G:
    case WebConnectionTypeCellular4G:
        return "cellular";
    case WebConnectionTypeBluetooth:
        return "bluetooth";
    case WebConnectionTypeEthernet:
        return "ethernet";
    case WebConnectionTypeWifi:
        return "wifi";
    case WebConnectionTypeWimax:
        return "wimax";
    case WebConnectionTypeOther:
        return "other";
    case WebConnectionTypeNone:
        return "none";
    case WebConnectionTypeUnknown:
        return "unknown";
    }
    ASSERT_NOT_REACHED();
    return "none";
}

} // namespace

namespace blink {

NetworkInformation* NetworkInformation::create(ExecutionContext* context)
{
    NetworkInformation* connection = new NetworkInformation(context);
    connection->suspendIfNeeded();
    return connection;
}

NetworkInformation::~NetworkInformation()
{
    ASSERT(!m_observing);
}

String NetworkInformation::type() const
{
    // m_type is only updated when listening for events, so ask networkStateNotifier
    // if not listening (crbug.com/379841).
    if (!m_observing)
        return connectionTypeToString(networkStateNotifier().connectionType());

    // If observing, return m_type which changes when the event fires, per spec.
    return connectionTypeToString(m_type);
}

double NetworkInformation::downlinkMax() const
{
    if (!m_observing)
        return networkStateNotifier().maxBandwidth();

    return m_downlinkMaxMbps;
}

void NetworkInformation::connectionChange(WebConnectionType type, double downlinkMaxMbps)
{
    ASSERT(getExecutionContext()->isContextThread());

    // This can happen if the observer removes and then adds itself again
    // during notification.
    if (m_type == type && m_downlinkMaxMbps == downlinkMaxMbps)
        return;

    m_type = type;
    m_downlinkMaxMbps = downlinkMaxMbps;
    dispatchEvent(Event::create(EventTypeNames::typechange));

    if (RuntimeEnabledFeatures::netInfoDownlinkMaxEnabled())
        dispatchEvent(Event::create(EventTypeNames::change));
}

const AtomicString& NetworkInformation::interfaceName() const
{
    return EventTargetNames::NetworkInformation;
}

ExecutionContext* NetworkInformation::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void NetworkInformation::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
    startObserving();
}

void NetworkInformation::removedEventListener(const AtomicString& eventType, const RegisteredEventListener& registeredListener)
{
    EventTargetWithInlineData::removedEventListener(eventType, registeredListener);
    if (!hasEventListeners())
        stopObserving();
}

void NetworkInformation::removeAllEventListeners()
{
    EventTargetWithInlineData::removeAllEventListeners();
    ASSERT(!hasEventListeners());
    stopObserving();
}

bool NetworkInformation::hasPendingActivity() const
{
    ASSERT(m_contextStopped || m_observing == hasEventListeners());

    // Prevent collection of this object when there are active listeners.
    return m_observing;
}

void NetworkInformation::stop()
{
    m_contextStopped = true;
    stopObserving();
}

void NetworkInformation::startObserving()
{
    if (!m_observing && !m_contextStopped) {
        m_type = networkStateNotifier().connectionType();
        networkStateNotifier().addObserver(this, getExecutionContext());
        m_observing = true;
    }
}

void NetworkInformation::stopObserving()
{
    if (m_observing) {
        networkStateNotifier().removeObserver(this, getExecutionContext());
        m_observing = false;
    }
}

NetworkInformation::NetworkInformation(ExecutionContext* context)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_type(networkStateNotifier().connectionType())
    , m_downlinkMaxMbps(networkStateNotifier().maxBandwidth())
    , m_observing(false)
    , m_contextStopped(false)
{
}

DEFINE_TRACE(NetworkInformation)
{
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
