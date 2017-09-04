// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorErrorEvent_h
#define SensorErrorEvent_h

#include "core/dom/DOMException.h"
#include "modules/EventModules.h"
#include "modules/sensor/SensorErrorEventInit.h"
#include "platform/heap/Handle.h"

namespace blink {

class SensorErrorEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SensorErrorEvent* create(const AtomicString& eventType,
                                  DOMException* error) {
    return new SensorErrorEvent(eventType, error);
  }

  static SensorErrorEvent* create(const AtomicString& eventType,
                                  const SensorErrorEventInit& initializer) {
    return new SensorErrorEvent(eventType, initializer);
  }

  ~SensorErrorEvent() override;

  DECLARE_VIRTUAL_TRACE();

  const AtomicString& interfaceName() const override;

  DOMException* error() { return m_error; }

 private:
  SensorErrorEvent(const AtomicString& eventType, DOMException* error);
  SensorErrorEvent(const AtomicString& eventType,
                   const SensorErrorEventInit& initializer);

  Member<DOMException> m_error;
};

DEFINE_TYPE_CASTS(SensorErrorEvent,
                  Event,
                  event,
                  event->interfaceName() == EventNames::SensorErrorEvent,
                  event.interfaceName() == EventNames::SensorErrorEvent);

}  // namepsace blink

#endif  // SensorErrorEvent_h
