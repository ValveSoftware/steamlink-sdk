// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/BatteryManager.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "modules/battery/BatteryDispatcher.h"
#include "wtf/Assertions.h"

namespace blink {

BatteryManager* BatteryManager::create(ExecutionContext* context)
{
    BatteryManager* batteryManager = new BatteryManager(context);
    batteryManager->suspendIfNeeded();
    return batteryManager;
}

BatteryManager::~BatteryManager()
{
}

BatteryManager::BatteryManager(ExecutionContext* context)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , PlatformEventController(toDocument(context)->page())
{
}

ScriptPromise BatteryManager::startRequest(ScriptState* scriptState)
{
    if (!m_batteryProperty) {
        m_batteryProperty = new BatteryProperty(scriptState->getExecutionContext(), this, BatteryProperty::Ready);

        // If the context is in a stopped state already, do not start updating.
        if (!getExecutionContext() || getExecutionContext()->activeDOMObjectsAreStopped()) {
            m_batteryProperty->resolve(this);
        } else {
            m_hasEventListener = true;
            startUpdating();
        }
    }

    return m_batteryProperty->promise(scriptState->world());
}

bool BatteryManager::charging()
{
    return m_batteryStatus.charging();
}

double BatteryManager::chargingTime()
{
    return m_batteryStatus.charging_time();
}

double BatteryManager::dischargingTime()
{
    return m_batteryStatus.discharging_time();
}

double BatteryManager::level()
{
    return m_batteryStatus.level();
}

void BatteryManager::didUpdateData()
{
    DCHECK(m_batteryProperty);

    BatteryStatus oldStatus = m_batteryStatus;
    m_batteryStatus = *BatteryDispatcher::instance().latestData();

    if (m_batteryProperty->getState() == ScriptPromisePropertyBase::Pending) {
        m_batteryProperty->resolve(this);
        return;
    }

    Document* document = toDocument(getExecutionContext());
    DCHECK(document);
    if (document->activeDOMObjectsAreSuspended() || document->activeDOMObjectsAreStopped())
        return;

    if (m_batteryStatus.charging() != oldStatus.charging())
        dispatchEvent(Event::create(EventTypeNames::chargingchange));
    if (m_batteryStatus.charging_time() != oldStatus.charging_time())
        dispatchEvent(Event::create(EventTypeNames::chargingtimechange));
    if (m_batteryStatus.discharging_time() != oldStatus.discharging_time())
        dispatchEvent(Event::create(EventTypeNames::dischargingtimechange));
    if (m_batteryStatus.level() != oldStatus.level())
        dispatchEvent(Event::create(EventTypeNames::levelchange));
}

void BatteryManager::registerWithDispatcher()
{
    BatteryDispatcher::instance().addController(this);
}

void BatteryManager::unregisterWithDispatcher()
{
    BatteryDispatcher::instance().removeController(this);
}

bool BatteryManager::hasLastData()
{
    return BatteryDispatcher::instance().latestData();
}

void BatteryManager::suspend()
{
    m_hasEventListener = false;
    stopUpdating();
}

void BatteryManager::resume()
{
    m_hasEventListener = true;
    startUpdating();
}

void BatteryManager::stop()
{
    m_hasEventListener = false;
    m_batteryProperty.clear();
    stopUpdating();
}

bool BatteryManager::hasPendingActivity() const
{
    // Prevent V8 from garbage collecting the wrapper object if there are
    // event listeners attached to it.
    return hasEventListeners();
}

DEFINE_TRACE(BatteryManager)
{
    visitor->trace(m_batteryProperty);
    PlatformEventController::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
