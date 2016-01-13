// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightEvent_h
#define DeviceLightEvent_h

#include "modules/EventModules.h"
#include "platform/heap/Handle.h"

namespace WebCore {

struct DeviceLightEventInit : public EventInit {
    DeviceLightEventInit()
        : value(std::numeric_limits<double>::infinity())
    {
        bubbles = true;
    };

    double value;
};

class DeviceLightEvent FINAL : public Event {
public:
    virtual ~DeviceLightEvent();

    static PassRefPtrWillBeRawPtr<DeviceLightEvent> create()
    {
        return adoptRefWillBeNoop(new DeviceLightEvent);
    }
    static PassRefPtrWillBeRawPtr<DeviceLightEvent> create(const AtomicString& eventType, double value)
    {
        return adoptRefWillBeNoop(new DeviceLightEvent(eventType, value));
    }
    static PassRefPtrWillBeRawPtr<DeviceLightEvent> create(const AtomicString& eventType, const DeviceLightEventInit& initializer)
    {
        return adoptRefWillBeNoop(new DeviceLightEvent(eventType, initializer));
    }

    double value() const { return m_value; }

    virtual const AtomicString& interfaceName() const OVERRIDE;

private:
    DeviceLightEvent();
    DeviceLightEvent(const AtomicString& eventType, double value);
    DeviceLightEvent(const AtomicString& eventType, const DeviceLightEventInit& initializer);

    double m_value;
};

DEFINE_TYPE_CASTS(DeviceLightEvent, Event, event, event->interfaceName() == EventNames::DeviceLightEvent, event.interfaceName() == EventNames::DeviceLightEvent);

} // namespace WebCore

#endif // DeviceLightEvent_h
