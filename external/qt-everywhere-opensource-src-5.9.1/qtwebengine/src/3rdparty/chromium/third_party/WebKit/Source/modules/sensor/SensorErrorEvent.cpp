// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorErrorEvent.h"

#include "bindings/core/v8/V8Binding.h"
#include <v8.h>

namespace blink {

SensorErrorEvent::~SensorErrorEvent() {}

SensorErrorEvent::SensorErrorEvent(const AtomicString& eventType,
                                   DOMException* error)
    : Event(eventType, false, false)  // does not bubble, is not cancelable.
      ,
      m_error(error) {
  DCHECK(m_error);
}

SensorErrorEvent::SensorErrorEvent(const AtomicString& eventType,
                                   const SensorErrorEventInit& initializer)
    : Event(eventType, initializer) {}

const AtomicString& SensorErrorEvent::interfaceName() const {
  return EventNames::SensorErrorEvent;
}

DEFINE_TRACE(SensorErrorEvent) {
  visitor->trace(m_error);
  Event::trace(visitor);
}

}  // namespace blink
