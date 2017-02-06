// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationEvent_h
#define NotificationEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/notifications/Notification.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"

namespace blink {

class NotificationEventInit;

class MODULES_EXPORT NotificationEvent final : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();
public:
    static NotificationEvent* create()
    {
        return new NotificationEvent;
    }
    static NotificationEvent* create(const AtomicString& type, const NotificationEventInit& initializer)
    {
        return new NotificationEvent(type, initializer);
    }
    static NotificationEvent* create(const AtomicString& type, const NotificationEventInit& initializer, WaitUntilObserver* observer)
    {
        return new NotificationEvent(type, initializer, observer);
    }

    ~NotificationEvent() override;

    Notification* getNotification() const { return m_notification.get(); }
    String action() const { return m_action; }

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    NotificationEvent();
    NotificationEvent(const AtomicString& type, const NotificationEventInit&);
    NotificationEvent(const AtomicString& type, const NotificationEventInit&, WaitUntilObserver*);

    Member<Notification> m_notification;
    String m_action;
};

} // namespace blink

#endif // NotificationEvent_h
