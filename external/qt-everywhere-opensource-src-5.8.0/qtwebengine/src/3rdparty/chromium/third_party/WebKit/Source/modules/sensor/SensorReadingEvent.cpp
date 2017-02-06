// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReadingEvent.h"

namespace blink {

SensorReadingEvent::~SensorReadingEvent()
{
}

SensorReadingEvent::SensorReadingEvent()
{
}

SensorReadingEvent::SensorReadingEvent(const AtomicString& eventType)
    : Event(eventType, true, false) // let default be bubbles but is not cancelable.
    , m_reading(SensorReading::create())
{
}

SensorReadingEvent::SensorReadingEvent(const AtomicString& eventType, SensorReading& reading)
    : Event(eventType, true, false) // let default be bubbles but is not cancelable.
    , m_reading(reading)
{
}

SensorReadingEvent::SensorReadingEvent(const AtomicString& eventType, const SensorReadingEventInit& initializer)
    : Event(eventType, initializer)
    , m_reading(SensorReading::create())
{
    setCanBubble(true);
}

const AtomicString& SensorReadingEvent::interfaceName() const
{
    return EventNames::SensorReadingEvent;
}

DEFINE_TRACE(SensorReadingEvent)
{
    Event::trace(visitor);
    visitor->trace(m_reading);
}

} // namespace blink
