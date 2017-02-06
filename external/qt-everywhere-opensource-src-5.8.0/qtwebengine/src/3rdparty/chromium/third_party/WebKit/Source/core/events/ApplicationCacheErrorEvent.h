// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ApplicationCacheErrorEvent_h
#define ApplicationCacheErrorEvent_h

#include "core/events/ApplicationCacheErrorEventInit.h"
#include "core/events/Event.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "public/platform/WebApplicationCacheHostClient.h"

namespace blink {

class ApplicationCacheErrorEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~ApplicationCacheErrorEvent() override;

    static ApplicationCacheErrorEvent* create()
    {
        return new ApplicationCacheErrorEvent;
    }

    static ApplicationCacheErrorEvent* create(WebApplicationCacheHost::ErrorReason reason, const String& url, int status, const String& message)
    {
        return new ApplicationCacheErrorEvent(reason, url, status, message);
    }

    static ApplicationCacheErrorEvent* create(const AtomicString& eventType, const ApplicationCacheErrorEventInit& initializer)
    {
        return new ApplicationCacheErrorEvent(eventType, initializer);
    }

    const String& reason() const { return m_reason; }
    const String& url() const { return m_url; }
    int status() const { return m_status; }
    const String& message() const { return m_message; }

    const AtomicString& interfaceName() const override { return EventNames::ApplicationCacheErrorEvent; }

    DECLARE_VIRTUAL_TRACE();

private:
    ApplicationCacheErrorEvent();
    ApplicationCacheErrorEvent(WebApplicationCacheHost::ErrorReason, const String& url, int status, const String& message);
    ApplicationCacheErrorEvent(const AtomicString& eventType, const ApplicationCacheErrorEventInit& initializer);

    String m_reason;
    String m_url;
    int m_status;
    String m_message;
};

} // namespace blink

#endif // ApplicationCacheErrorEvent_h
