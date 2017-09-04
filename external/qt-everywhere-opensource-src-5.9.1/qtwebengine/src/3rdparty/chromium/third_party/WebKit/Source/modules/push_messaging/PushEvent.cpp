// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushEvent.h"

#include "modules/push_messaging/PushEventInit.h"

namespace blink {

PushEvent::PushEvent(const AtomicString& type,
                     PushMessageData* data,
                     WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer), m_data(data) {}

PushEvent::PushEvent(const AtomicString& type, const PushEventInit& initializer)
    : ExtendableEvent(type, initializer) {
  if (initializer.hasData())
    m_data = PushMessageData::create(initializer.data());
}

PushEvent::~PushEvent() {}

const AtomicString& PushEvent::interfaceName() const {
  return EventNames::PushEvent;
}

PushMessageData* PushEvent::data() {
  return m_data.get();
}

DEFINE_TRACE(PushEvent) {
  visitor->trace(m_data);
  ExtendableEvent::trace(visitor);
}

}  // namespace blink
