// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorReadingEvent_h
#define SensorReadingEvent_h

#include "modules/EventModules.h"
#include "modules/sensor/SensorReading.h"
#include "modules/sensor/SensorReadingEventInit.h"
#include "platform/heap/Handle.h"

namespace blink {

class SensorReadingEvent : public Event {
    DEFINE_WRAPPERTYPEINFO();

public:
    static SensorReadingEvent* create()
    {
        return new SensorReadingEvent;
    }

    static SensorReadingEvent* create(const AtomicString& eventType)
    {
        return new SensorReadingEvent(eventType);
    }

    static SensorReadingEvent* create(const AtomicString& eventType, SensorReading& reading)
    {
        return new SensorReadingEvent(eventType, reading);
    }

    static SensorReadingEvent* create(const AtomicString& eventType, const SensorReadingEventInit& initializer)
    {
        return new SensorReadingEvent(eventType, initializer);
    }

    ~SensorReadingEvent() override;

    // TODO(riju): crbug.com/614797 .
    SensorReading* reading() const { return m_reading.get(); }
    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

protected:
    Member<SensorReading> m_reading;

private:
    SensorReadingEvent();
    explicit SensorReadingEvent(const AtomicString& eventType);
    SensorReadingEvent(const AtomicString& eventType, SensorReading&);
    SensorReadingEvent(const AtomicString& eventType, const SensorReadingEventInit& initializer);

};

DEFINE_TYPE_CASTS(SensorReadingEvent, Event, event, event->interfaceName() == EventNames::SensorReadingEvent, event.interfaceName() == EventNames::SensorReadingEvent);

} // namepsace blink

#endif // SensorReadingEvent_h
