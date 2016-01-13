// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ApplicationCacheErrorEvent_h
#define ApplicationCacheErrorEvent_h

#include "core/events/Event.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "public/platform/WebApplicationCacheHostClient.h"

namespace WebCore {

class ApplicationCacheErrorEvent;

struct ApplicationCacheErrorEventInit : public EventInit {
    ApplicationCacheErrorEventInit();

    String reason;
    String url;
    int status;
    String message;
};

class ApplicationCacheErrorEvent FINAL : public Event {
public:
    virtual ~ApplicationCacheErrorEvent();

    static PassRefPtrWillBeRawPtr<ApplicationCacheErrorEvent> create()
    {
        return adoptRefWillBeNoop(new ApplicationCacheErrorEvent);
    }

    static PassRefPtrWillBeRawPtr<ApplicationCacheErrorEvent> create(blink::WebApplicationCacheHost::ErrorReason reason, const String& url, int status, const String& message)
    {
        return adoptRefWillBeNoop(new ApplicationCacheErrorEvent(reason, url, status, message));
    }

    static PassRefPtrWillBeRawPtr<ApplicationCacheErrorEvent> create(const AtomicString& eventType, const ApplicationCacheErrorEventInit& initializer)
    {
        return adoptRefWillBeNoop(new ApplicationCacheErrorEvent(eventType, initializer));
    }

    const String& reason() const { return m_reason; }
    const String& url() const { return m_url; }
    int status() const { return m_status; }
    const String& message() const { return m_message; }

    virtual const AtomicString& interfaceName() const OVERRIDE { return EventNames::ApplicationCacheErrorEvent; }

    virtual void trace(Visitor*) OVERRIDE;

private:
    ApplicationCacheErrorEvent();
    ApplicationCacheErrorEvent(blink::WebApplicationCacheHost::ErrorReason, const String& url, int status, const String& message);
    ApplicationCacheErrorEvent(const AtomicString& eventType, const ApplicationCacheErrorEventInit& initializer);

    String m_reason;
    String m_url;
    int m_status;
    String m_message;
};

} // namespace WebCore

#endif // ApplicationCacheErrorEvent_h
