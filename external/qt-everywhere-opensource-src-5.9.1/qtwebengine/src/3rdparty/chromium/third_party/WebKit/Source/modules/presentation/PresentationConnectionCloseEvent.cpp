// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionCloseEvent.h"

#include "modules/presentation/PresentationConnectionCloseEventInit.h"

namespace blink {

PresentationConnectionCloseEvent::PresentationConnectionCloseEvent(
    const AtomicString& eventType,
    const String& reason,
    const String& message)
    : Event(eventType, false /* canBubble */, false /* cancelable */),
      m_reason(reason),
      m_message(message) {}

PresentationConnectionCloseEvent::PresentationConnectionCloseEvent(
    const AtomicString& eventType,
    const PresentationConnectionCloseEventInit& initializer)
    : Event(eventType, initializer),
      m_reason(initializer.reason()),
      m_message(initializer.message()) {}

const AtomicString& PresentationConnectionCloseEvent::interfaceName() const {
  return EventNames::PresentationConnectionCloseEvent;
}

DEFINE_TRACE(PresentationConnectionCloseEvent) {
  Event::trace(visitor);
}

}  // namespace blink
