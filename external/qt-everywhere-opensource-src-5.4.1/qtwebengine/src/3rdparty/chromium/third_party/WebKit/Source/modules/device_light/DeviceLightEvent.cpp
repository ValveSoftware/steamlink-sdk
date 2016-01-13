// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_light/DeviceLightEvent.h"

namespace WebCore {

DeviceLightEvent::~DeviceLightEvent()
{
}

DeviceLightEvent::DeviceLightEvent()
    : m_value(std::numeric_limits<double>::infinity())
{
    ScriptWrappable::init(this);
}

DeviceLightEvent::DeviceLightEvent(const AtomicString& eventType, double value)
    : Event(eventType, true, false) // The DeviceLightEvent bubbles but is not cancelable.
    , m_value(value)
{
    ScriptWrappable::init(this);
}

DeviceLightEvent::DeviceLightEvent(const AtomicString& eventType, const DeviceLightEventInit& initializer)
    : Event(eventType, initializer)
    , m_value(initializer.value)
{
    ScriptWrappable::init(this);
}

const AtomicString& DeviceLightEvent::interfaceName() const
{
    return EventNames::DeviceLightEvent;
}

} // namespace WebCore




