/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "modules/notifications/Notification.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "modules/notifications/NotificationClient.h"
#include "modules/notifications/NotificationController.h"

namespace WebCore {

Notification* Notification::create(ExecutionContext* context, const String& title, const Dictionary& options)
{
    NotificationClient& client = NotificationController::clientFrom(toDocument(context)->frame());
    Notification* notification = adoptRefCountedGarbageCollectedWillBeNoop(new Notification(title, context, &client));

    String argument;
    if (options.get("body", argument))
        notification->setBody(argument);
    if (options.get("tag", argument))
        notification->setTag(argument);
    if (options.get("lang", argument))
        notification->setLang(argument);
    if (options.get("dir", argument))
        notification->setDir(argument);
    if (options.get("icon", argument)) {
        KURL iconUrl = argument.isEmpty() ? KURL() : context->completeURL(argument);
        if (!iconUrl.isEmpty() && iconUrl.isValid())
            notification->setIconUrl(iconUrl);
    }

    notification->suspendIfNeeded();
    return notification;
}

Notification::Notification(const String& title, ExecutionContext* context, NotificationClient* client)
    : ActiveDOMObject(context)
    , m_title(title)
    , m_dir("auto")
    , m_state(Idle)
    , m_client(client)
    , m_asyncRunner(adoptPtr(new AsyncMethodRunner<Notification>(this, &Notification::show)))
{
    ASSERT(m_client);
    ScriptWrappable::init(this);

    m_asyncRunner->runAsync();
}

Notification::~Notification()
{
}

void Notification::show()
{
    ASSERT(m_state == Idle);
    if (!toDocument(executionContext())->page())
        return;

    if (m_client->checkPermission(executionContext()) != NotificationClient::PermissionAllowed) {
        dispatchErrorEvent();
        return;
    }

    if (m_client->show(this))
        m_state = Showing;
}

void Notification::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing:
        m_client->close(this);
        break;
    case Closed:
        break;
    }
}

void Notification::dispatchShowEvent()
{
    dispatchEvent(Event::create(EventTypeNames::show));
}

void Notification::dispatchClickEvent()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    WindowFocusAllowedIndicator windowFocusAllowed;
    dispatchEvent(Event::create(EventTypeNames::click));
}

void Notification::dispatchErrorEvent()
{
    dispatchEvent(Event::create(EventTypeNames::error));
}

void Notification::dispatchCloseEvent()
{
    dispatchEvent(Event::create(EventTypeNames::close));
    m_state = Closed;
}

TextDirection Notification::direction() const
{
    // FIXME: Resolve dir()=="auto" against the document.
    return dir() == "rtl" ? RTL : LTR;
}

const String& Notification::permissionString(NotificationClient::Permission permission)
{
    DEFINE_STATIC_LOCAL(const String, allowedPermission, ("granted"));
    DEFINE_STATIC_LOCAL(const String, deniedPermission, ("denied"));
    DEFINE_STATIC_LOCAL(const String, defaultPermission, ("default"));

    switch (permission) {
    case NotificationClient::PermissionAllowed:
        return allowedPermission;
    case NotificationClient::PermissionDenied:
        return deniedPermission;
    case NotificationClient::PermissionNotAllowed:
        return defaultPermission;
    }

    ASSERT_NOT_REACHED();
    return deniedPermission;
}

const String& Notification::permission(ExecutionContext* context)
{
    ASSERT(toDocument(context)->page());

    UseCounter::count(context, UseCounter::NotificationPermission);
    return permissionString(NotificationController::clientFrom(toDocument(context)->frame()).checkPermission(context));
}

void Notification::requestPermission(ExecutionContext* context, PassOwnPtr<NotificationPermissionCallback> callback)
{
    ASSERT(toDocument(context)->page());
    NotificationController::clientFrom(toDocument(context)->frame()).requestPermission(context, callback);
}

bool Notification::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    ASSERT(m_state != Closed);

    return EventTarget::dispatchEvent(event);
}

const AtomicString& Notification::interfaceName() const
{
    return EventTargetNames::Notification;
}

void Notification::stop()
{
    if (m_client)
        m_client->notificationObjectDestroyed(this);

    if (m_asyncRunner)
        m_asyncRunner->stop();

    m_client = 0;
    m_state = Closed;
}

bool Notification::hasPendingActivity() const
{
    return m_state == Showing || (m_asyncRunner && m_asyncRunner->isActive());
}

} // namespace WebCore
