// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationEvent.h"

#include "modules/notifications/NotificationEventInit.h"
#include "wtf/text/WTFString.h"

namespace blink {

NotificationEvent::NotificationEvent()
    : m_action(emptyString())
{
}

NotificationEvent::NotificationEvent(const AtomicString& type, const NotificationEventInit& initializer)
    : ExtendableEvent(type, initializer)
    , m_action(initializer.action())
{
    if (initializer.hasNotification())
        m_notification = initializer.notification();
}

NotificationEvent::NotificationEvent(const AtomicString& type, const NotificationEventInit& initializer, WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer)
    , m_action(initializer.action())
{
    if (initializer.hasNotification())
        m_notification = initializer.notification();
}

NotificationEvent::~NotificationEvent()
{
}

const AtomicString& NotificationEvent::interfaceName() const
{
    return EventNames::NotificationEvent;
}

DEFINE_TRACE(NotificationEvent)
{
    visitor->trace(m_notification);
    ExtendableEvent::trace(visitor);
}

} // namespace blink
