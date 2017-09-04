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
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"

namespace blink {

class ExecutionContext;
class NotificationOptions;
class NotificationPermissionCallback;
class NotificationResourcesLoader;
class ScriptState;

class MODULES_EXPORT Notification final : public EventTargetWithInlineData,
                                          public ActiveScriptWrappable,
                                          public ActiveDOMObject,
                                          public WebNotificationDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(Notification);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Used for JavaScript instantiations of non-persistent notifications. Will
  // automatically schedule for the notification to be displayed to the user
  // when the developer-provided data is valid.
  static Notification* create(ExecutionContext*,
                              const String& title,
                              const NotificationOptions&,
                              ExceptionState&);

  // Used for embedder-created persistent notifications. Initializes the state
  // of the notification as either Showing or Closed based on |showing|.
  static Notification* create(ExecutionContext*,
                              const String& notificationId,
                              const WebNotificationData&,
                              bool showing);

  ~Notification() override;

  void close();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(show);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

  // WebNotificationDelegate interface.
  void dispatchShowEvent() override;
  void dispatchClickEvent() override;
  void dispatchErrorEvent() override;
  void dispatchCloseEvent() override;

  String title() const;
  String dir() const;
  String lang() const;
  String body() const;
  String tag() const;
  String image() const;
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
  static ScriptPromise requestPermission(ScriptState*,
                                         NotificationPermissionCallback*);

  static size_t maxActions();

  // EventTarget interface.
  ExecutionContext* getExecutionContext() const final {
    return ActiveDOMObject::getExecutionContext();
  }
  const AtomicString& interfaceName() const override;

  // ActiveDOMObject interface.
  void contextDestroyed() override;

  // ScriptWrappable interface.
  bool hasPendingActivity() const final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget interface.
  DispatchEventResult dispatchEventInternal(Event*) final;

 private:
  // The type of notification this instance represents. Non-persistent
  // notifications will have events delivered to their instance, whereas
  // persistent notification will be using a Service Worker.
  enum class Type { NonPersistent, Persistent };

  // The current phase of the notification in its lifecycle.
  enum class State { Loading, Showing, Closing, Closed };

  Notification(ExecutionContext*, Type, const WebNotificationData&);

  // Sets the state of the notification in its lifecycle.
  void setState(State state) { m_state = state; }

  // Sets the notification ID to |notificationId|. This should be done once
  // the notification has shown for non-persistent notifications, and at
  // object initialisation time for persistent notifications.
  void setNotificationId(const String& notificationId) {
    m_notificationId = notificationId;
  }

  // Schedules an asynchronous call to |prepareShow|, allowing the constructor
  // to return so that events can be fired on the notification object.
  void schedulePrepareShow();

  // Verifies that permission has been granted, then asynchronously starts
  // loading the resources associated with this notification.
  void prepareShow();

  // Shows the notification through the embedder using the loaded resources.
  void didLoadResources(NotificationResourcesLoader*);

  Type m_type;
  State m_state;

  WebNotificationData m_data;

  String m_notificationId;

  Member<AsyncMethodRunner<Notification>> m_prepareShowMethodRunner;

  Member<NotificationResourcesLoader> m_loader;
};

}  // namespace blink

#endif  // Notification_h
