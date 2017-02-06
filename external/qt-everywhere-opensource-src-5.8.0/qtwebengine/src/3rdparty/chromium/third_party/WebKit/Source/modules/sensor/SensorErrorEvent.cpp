// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorErrorEvent.h"

#include "bindings/core/v8/V8Binding.h"
#include <v8.h>

namespace blink {

SensorErrorEvent::~SensorErrorEvent()
{
}

SensorErrorEvent::SensorErrorEvent()
    : Event(EventTypeNames::error, false, true)
{
}

SensorErrorEvent::SensorErrorEvent(const AtomicString& eventType)
    : Event(eventType, true, false) // let default be bubbles but is not cancelable.
{
}

SensorErrorEvent::SensorErrorEvent(const AtomicString& eventType, const SensorErrorEventInit& initializer)
    : Event(eventType, initializer)
{
    setCanBubble(true);
}

const AtomicString& SensorErrorEvent::interfaceName() const
{
    return EventNames::SensorErrorEvent;
}

DEFINE_TRACE(SensorErrorEvent)
{
    Event::trace(visitor);
}

} // namespace blink
