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

#ifndef Notification_h
#define Notification_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMTimeStamp.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/vibration/NavigatorVibration.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationDelegate.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"

namespace blink {

class ExecutionContext;
class NotificationAction;
class NotificationManager;
class NotificationOptions;
class NotificationPermissionCallback;
class NotificationResourcesLoader;
class ScriptState;

class MODULES_EXPORT Notification final : public EventTargetWithInlineData, public ActiveScriptWrappable, public ActiveDOMObject, public WebNotificationDelegate {
    USING_GARBAGE_COLLECTED_MIXIN(Notification);
    DEFINE_WRAPPERTYPEINFO();
public:
    // Used for JavaScript instantiations of the Notification object. Will automatically schedule for
    // the notification to be displayed to the user when the developer-provided data is valid.
    static Notification* create(ExecutionContext*, const String& title, const NotificationOptions&, ExceptionState&);

    // Used for embedder-created Notification objects. If |showing| is true, will initialize the
    // Notification's state as showing, or as closed otherwise.
    static Notification* create(ExecutionContext*, int64_t persistentId, const WebNotificationData&, bool showing);

    ~Notification() override;

    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(show);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

    // WebNotificationDelegate implementation.
    void dispatchShowEvent() override;
    void dispatchClickEvent() override;
    void dispatchErrorEvent() override;
    void dispatchCloseEvent() override;

    String title() const;
    String dir() const;
    String lang() const;
    String body() const;
    String tag() const;
    String icon() const;
    String badge() const;
    NavigatorVibration::VibrationPattern vibrate() const;
    DOMTimeStamp timestamp() const;
    bool renotify() const;
    bool silent() const;
    bool requireInteraction() const;
    ScriptValue data(ScriptState*);
    Vector<v8::Local<v8::Value>> actions(ScriptState*) const;

    static String permissionString(mojom::blink::PermissionStatus);
    static String permission(ExecutionContext*);
    static ScriptPromise requestPermission(ScriptState*, NotificationPermissionCallback*);

    static size_t maxActions();

    // EventTarget interface.
    ExecutionContext* getExecutionContext() const final { return ActiveDOMObject::getExecutionContext(); }
    const AtomicString& interfaceName() const override;

    // ActiveDOMObject interface.
    void stop() override;

    // ActiveScriptWrappable interface.
    bool hasPendingActivity() const final;

    DECLARE_VIRTUAL_TRACE();

protected:
    // EventTarget interface.
    DispatchEventResult dispatchEventInternal(Event*) final;

private:
    Notification(ExecutionContext*, const WebNotificationData&);

    // Schedules an asynchronous call to |prepareShow|, allowing the constructor
    // to return so that events can be fired on the notification object.
    void schedulePrepareShow();

    // Checks permission and loads any necessary resources (this may be async)
    // before showing the notification.
    void prepareShow();

    // Shows the notification, using the resources loaded by the
    // NotificationResourcesLoader.
    void didLoadResources(NotificationResourcesLoader*);

    void setPersistentId(int64_t persistentId) { m_persistentId = persistentId; }

    WebNotificationData m_data;

    // ScriptValue representations of the developer-associated data. Initialized lazily on first access.
    ScriptValue m_developerData;

    // Notifications can either be bound to the page, which means they're identified by
    // their delegate, or persistent, which means they're identified by a persistent Id
    // given to us by the embedder. This influences how we close the notification.
    int64_t m_persistentId;

    enum NotificationState {
        NotificationStateIdle,
        NotificationStateShowing,
        NotificationStateClosing,
        NotificationStateClosed
    };

    // Only to be used by the Notification::create() method when notifications were created
    // by the embedder rather than by Blink.
    void setState(NotificationState state) { m_state = state; }

    NotificationState m_state;

    Member<AsyncMethodRunner<Notification>> m_prepareShowMethodRunner;

    Member<NotificationResourcesLoader> m_loader;
};

} // namespace blink

#endif // Notification_h
