// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Sensor.h"

#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/Event.h"

#include "modules/sensor/SensorReading.h"

namespace blink {

Sensor::~Sensor()
{
}

Sensor::Sensor(ExecutionContext* executionContext, const SensorOptions& sensorOptions)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(executionContext)
    , PlatformEventController(toDocument(executionContext)->page())
    , m_sensorState(SensorState::Idle)
    , m_sensorReading(nullptr)
    , m_sensorOptions(sensorOptions)
{
}

// Getters
String Sensor::state() const
{
    // TODO(riju): Validate the transitions.
    switch (m_sensorState) {
    case SensorState::Idle:
        return "idle";
    case SensorState::Activating:
        return "activating";
    case SensorState::Active:
        return "active";
    case SensorState::Errored:
        return "errored";
    }
    NOTREACHED();
    return "idle";
}

SensorReading* Sensor::reading() const
{
    return m_sensorReading.get();
}

void Sensor::start(ScriptState* scriptState, ExceptionState& exceptionState)
{

    if (m_sensorState != SensorState::Idle && m_sensorState != SensorState::Errored) {
        exceptionState.throwDOMException(InvalidStateError, "Invalid State: SensorState is not idle or errored");
        return;
    }

    updateState(SensorState::Activating);

    // TODO(riju) : Add Permissions stuff later.

    m_hasEventListener = true;

    // TODO(riju): verify the correct order of onstatechange(active) and the first onchange(event).
    startUpdating();
}

void Sensor::stop(ScriptState* scriptState, ExceptionState& exceptionState)
{
    if (m_sensorState == SensorState::Idle || m_sensorState == SensorState::Errored) {
        exceptionState.throwDOMException(InvalidStateError, "Invalid State: SensorState is either idle or errored");
        return;
    }

    m_hasEventListener = false;
    stopUpdating();

    m_sensorReading.clear();
    updateState(SensorState::Idle);
}

void Sensor::updateState(SensorState newState)
{
    DCHECK(isMainThread());
    if (m_sensorState == newState)
        return;

    m_sensorState = newState;
    // Notify context that state changed.
    if (getExecutionContext())
        getExecutionContext()->postTask(BLINK_FROM_HERE, createSameThreadTask(&Sensor::notifyStateChange, wrapPersistent(this)));
}

void Sensor::notifyStateChange()
{
    dispatchEvent(Event::create(EventTypeNames::statechange));
}

void Sensor::suspend()
{
    m_hasEventListener = false;
    stopUpdating();
}

void Sensor::resume()
{
    m_hasEventListener = true;
    startUpdating();
}

void Sensor::stop()
{
    m_hasEventListener = false;
    stopUpdating();
}

bool Sensor::hasPendingActivity() const
{
    // Prevent V8 from garbage collecting the wrapper object if there are
    // event listeners attached to it.
    return hasEventListeners();
}

DEFINE_TRACE(Sensor)
{
    ActiveDOMObject::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
    PlatformEventController::trace(visitor);
    visitor->trace(m_sensorReading);
}

} // namespace blink
