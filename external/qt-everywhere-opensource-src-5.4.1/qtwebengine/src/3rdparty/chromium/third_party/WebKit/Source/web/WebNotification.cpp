/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "public/web/WebNotification.h"

#include "core/events/Event.h"
#include "modules/notifications/Notification.h"
#include "public/platform/WebURL.h"
#include "wtf/PassRefPtr.h"

using WebCore::Notification;

namespace blink {

void WebNotification::reset()
{
    m_private.reset();
}

void WebNotification::assign(const WebNotification& other)
{
    m_private = other.m_private;
}

bool WebNotification::equals(const WebNotification& other) const
{
    return m_private.get() == other.m_private.get();
}

bool WebNotification::lessThan(const WebNotification& other) const
{
    return m_private.get() < other.m_private.get();
}

WebString WebNotification::title() const
{
    return m_private->title();
}

WebTextDirection WebNotification::direction() const
{
    return (m_private->direction() == WebCore::RTL) ?
        WebTextDirectionRightToLeft :
        WebTextDirectionLeftToRight;
}

WebString WebNotification::lang() const
{
    return m_private->lang();
}

WebString WebNotification::body() const
{
    return m_private->body();
}

WebString WebNotification::tag() const
{
    return m_private->tag();
}

WebURL WebNotification::iconURL() const
{
    return m_private->iconURL();
}

void WebNotification::dispatchShowEvent()
{
    m_private->dispatchShowEvent();
}

void WebNotification::dispatchErrorEvent(const WebString& /* errorMessage */)
{
    // FIXME: errorMessage not supported by WebCore yet
    m_private->dispatchErrorEvent();
}

void WebNotification::dispatchCloseEvent(bool /* byUser */)
{
    // FIXME: byUser flag not supported by WebCore yet
    m_private->dispatchCloseEvent();
}

void WebNotification::dispatchClickEvent()
{
    m_private->dispatchClickEvent();
}

WebNotification::WebNotification(Notification* notification)
    : m_private(notification)
{
}

WebNotification& WebNotification::operator=(Notification* notification)
{
    m_private = notification;
    return *this;
}

} // namespace blink
