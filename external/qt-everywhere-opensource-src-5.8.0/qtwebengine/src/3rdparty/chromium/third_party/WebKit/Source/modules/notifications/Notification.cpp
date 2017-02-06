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

#include "modules/notifications/Notification.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/modules/v8/V8NotificationAction.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/ScopedWindowFocusAllowedIndicator.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "modules/notifications/NotificationAction.h"
#include "modules/notifications/NotificationData.h"
#include "modules/notifications/NotificationManager.h"
#include "modules/notifications/NotificationOptions.h"
#include "modules/notifications/NotificationPermissionClient.h"
#include "modules/notifications/NotificationResourcesLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/notifications/WebNotificationAction.h"
#include "public/platform/modules/notifications/WebNotificationConstants.h"
#include "public/platform/modules/notifications/WebNotificationManager.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"

namespace blink {
namespace {

const int64_t kInvalidPersistentId = -1;

WebNotificationManager* notificationManager()
{
    return Platform::current()->notificationManager();
}

} // namespace

Notification* Notification::create(ExecutionContext* context, const String& title, const NotificationOptions& options, ExceptionState& exceptionState)
{
    // The Web Notification constructor may be disabled through a runtime feature. The
    // behavior of the constructor is changing, but not completely agreed upon yet.
    if (!RuntimeEnabledFeatures::notificationConstructorEnabled()) {
        exceptionState.throwTypeError("Illegal constructor. Use ServiceWorkerRegistration.showNotification() instead.");
        return nullptr;
    }

    // The Web Notification constructor may not be used in Service Worker contexts.
    if (context->isServiceWorkerGlobalScope()) {
        exceptionState.throwTypeError("Illegal constructor.");
        return nullptr;
    }

    if (!options.actions().isEmpty()) {
        exceptionState.throwTypeError("Actions are only supported for persistent notifications shown using ServiceWorkerRegistration.showNotification().");
        return nullptr;
    }

    String insecureOriginMessage;
    if (context->isSecureContext(insecureOriginMessage)) {
        UseCounter::count(context, UseCounter::NotificationSecureOrigin);
        if (context->isDocument())
            UseCounter::countCrossOriginIframe(*toDocument(context),  UseCounter::NotificationAPISecureOriginIframe);
    } else {
        UseCounter::count(context, UseCounter::NotificationInsecureOrigin);
        if (context->isDocument())
            UseCounter::countCrossOriginIframe(*toDocument(context),  UseCounter::NotificationAPIInsecureOriginIframe);
    }

    WebNotificationData data = createWebNotificationData(context, title, options, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    Notification* notification = new Notification(context, data);
    notification->schedulePrepareShow();
    notification->suspendIfNeeded();

    return notification;
}

Notification* Notification::create(ExecutionContext* context, int64_t persistentId, const WebNotificationData& data, bool showing)
{
    Notification* notification = new Notification(context, data);
    notification->setPersistentId(persistentId);
    notification->setState(showing ? NotificationStateShowing : NotificationStateClosed);
    notification->suspendIfNeeded();

    return notification;
}

Notification::Notification(ExecutionContext* context, const WebNotificationData& data)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_data(data)
    , m_persistentId(kInvalidPersistentId)
    , m_state(NotificationStateIdle)
    , m_prepareShowMethodRunner(AsyncMethodRunner<Notification>::create(this, &Notification::prepareShow))
{
    DCHECK(notificationManager());
}

Notification::~Notification()
{
}

void Notification::schedulePrepareShow()
{
    DCHECK_EQ(m_state, NotificationStateIdle);
    DCHECK(!m_prepareShowMethodRunner->isActive());

    m_prepareShowMethodRunner->runAsync();
}

void Notification::prepareShow()
{
    DCHECK_EQ(m_state, NotificationStateIdle);
    if (NotificationManager::from(getExecutionContext())->permissionStatus() != mojom::blink::PermissionStatus::GRANTED) {
        dispatchErrorEvent();
        return;
    }

    m_loader = new NotificationResourcesLoader(WTF::bind(&Notification::didLoadResources, wrapWeakPersistent(this)));
    m_loader->start(getExecutionContext(), m_data);
}

void Notification::didLoadResources(NotificationResourcesLoader* loader)
{
    DCHECK_EQ(loader, m_loader.get());

    SecurityOrigin* origin = getExecutionContext()->getSecurityOrigin();
    DCHECK(origin);

    notificationManager()->show(WebSecurityOrigin(origin), m_data, loader->getResources(), this);
    m_loader.clear();

    m_state = NotificationStateShowing;
}

void Notification::close()
{
    if (m_state != NotificationStateShowing)
        return;

    if (m_persistentId == kInvalidPersistentId) {
        // Fire the close event asynchronously.
        getExecutionContext()->postTask(BLINK_FROM_HERE, createSameThreadTask(&Notification::dispatchCloseEvent, wrapPersistent(this)));

        m_state = NotificationStateClosing;
        notificationManager()->close(this);
    } else {
        m_state = NotificationStateClosed;

        SecurityOrigin* origin = getExecutionContext()->getSecurityOrigin();
        DCHECK(origin);

        notificationManager()->closePersistent(WebSecurityOrigin(origin), m_persistentId);
    }
}

void Notification::dispatchShowEvent()
{
    dispatchEvent(Event::create(EventTypeNames::show));
}

void Notification::dispatchClickEvent()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    ScopedWindowFocusAllowedIndicator windowFocusAllowed(getExecutionContext());
    dispatchEvent(Event::create(EventTypeNames::click));
}

void Notification::dispatchErrorEvent()
{
    dispatchEvent(Event::create(EventTypeNames::error));
}

void Notification::dispatchCloseEvent()
{
    // The notification will be showing when the user initiated the close, or it will be
    // closing if the developer initiated the close.
    if (m_state != NotificationStateShowing && m_state != NotificationStateClosing)
        return;

    m_state = NotificationStateClosed;
    dispatchEvent(Event::create(EventTypeNames::close));
}

String Notification::title() const
{
    return m_data.title;
}

String Notification::dir() const
{
    switch (m_data.direction) {
    case WebNotificationData::DirectionLeftToRight:
        return "ltr";
    case WebNotificationData::DirectionRightToLeft:
        return "rtl";
    case WebNotificationData::DirectionAuto:
        return "auto";
    }

    NOTREACHED();
    return String();
}

String Notification::lang() const
{
    return m_data.lang;
}

String Notification::body() const
{
    return m_data.body;
}

String Notification::tag() const
{
    return m_data.tag;
}

String Notification::icon() const
{
    return m_data.icon.string();
}

String Notification::badge() const
{
    return m_data.badge.string();
}

NavigatorVibration::VibrationPattern Notification::vibrate() const
{
    NavigatorVibration::VibrationPattern pattern;
    pattern.appendRange(m_data.vibrate.begin(), m_data.vibrate.end());

    return pattern;
}

DOMTimeStamp Notification::timestamp() const
{
    return m_data.timestamp;
}

bool Notification::renotify() const
{
    return m_data.renotify;
}

bool Notification::silent() const
{
    return m_data.silent;
}

bool Notification::requireInteraction() const
{
    return m_data.requireInteraction;
}

ScriptValue Notification::data(ScriptState* scriptState)
{
    if (m_developerData.isEmpty()) {
        const WebVector<char>& serializedData = m_data.data;
        RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(serializedData.data(), serializedData.size());
        m_developerData = ScriptValue(scriptState, serializedValue->deserialize(scriptState->isolate()));
    }

    return m_developerData;
}

Vector<v8::Local<v8::Value>> Notification::actions(ScriptState* scriptState) const
{
    Vector<v8::Local<v8::Value>> actions;
    actions.grow(m_data.actions.size());

    for (size_t i = 0; i < m_data.actions.size(); ++i) {
        NotificationAction action;

        switch (m_data.actions[i].type) {
        case WebNotificationAction::Button:
            action.setType("button");
            break;
        case WebNotificationAction::Text:
            action.setType("text");
            break;
        default:
            NOTREACHED() << "Unknown action type: " << m_data.actions[i].type;
        }

        action.setAction(m_data.actions[i].action);
        action.setTitle(m_data.actions[i].title);
        action.setIcon(m_data.actions[i].icon.string());
        action.setPlaceholder(m_data.actions[i].placeholder);

        // Not just the sequence of actions itself, but also the actions contained within the
        // sequence should be frozen per the Web Notification specification.
        actions[i] = freezeV8Object(toV8(action, scriptState), scriptState->isolate());
    }

    return actions;
}

String Notification::permissionString(mojom::blink::PermissionStatus permission)
{
    switch (permission) {
    case mojom::blink::PermissionStatus::GRANTED:
        return "granted";
    case mojom::blink::PermissionStatus::DENIED:
        return "denied";
    case mojom::blink::PermissionStatus::ASK:
        return "default";
    }

    NOTREACHED();
    return "denied";
}

String Notification::permission(ExecutionContext* context)
{
    return permissionString(NotificationManager::from(context)->permissionStatus());
}

ScriptPromise Notification::requestPermission(ScriptState* scriptState, NotificationPermissionCallback* deprecatedCallback)
{
    ExecutionContext* context = scriptState->getExecutionContext();
    if (NotificationPermissionClient* permissionClient = NotificationPermissionClient::from(context))
        return permissionClient->requestPermission(scriptState, deprecatedCallback);

    // The context has been detached. Return a promise that will never settle.
    DCHECK(context->activeDOMObjectsAreStopped());

    return ScriptPromise();
}

size_t Notification::maxActions()
{
    return kWebNotificationMaxActions;
}

DispatchEventResult Notification::dispatchEventInternal(Event* event)
{
    DCHECK(getExecutionContext()->isContextThread());
    return EventTarget::dispatchEventInternal(event);
}

const AtomicString& Notification::interfaceName() const
{
    return EventTargetNames::Notification;
}

void Notification::stop()
{
    notificationManager()->notifyDelegateDestroyed(this);

    m_state = NotificationStateClosed;

    m_prepareShowMethodRunner->stop();

    if (m_loader)
        m_loader->stop();
}

bool Notification::hasPendingActivity() const
{
    return m_state == NotificationStateShowing || m_prepareShowMethodRunner->isActive() || m_loader;
}

DEFINE_TRACE(Notification)
{
    visitor->trace(m_prepareShowMethodRunner);
    visitor->trace(m_loader);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
