// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistrationNotifications_h
#define ServiceWorkerRegistrationNotifications_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/Supplementable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Visitor.h"
#include "public/platform/modules/notifications/WebNotificationManager.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include <memory>

namespace blink {

class ExecutionContext;
class ExceptionState;
class GetNotificationOptions;
class NotificationOptions;
class NotificationResourcesLoader;
class ScriptState;
class SecurityOrigin;
class ServiceWorkerRegistration;
struct WebNotificationData;

class ServiceWorkerRegistrationNotifications final : public GarbageCollected<ServiceWorkerRegistrationNotifications>, public Supplement<ServiceWorkerRegistration>, public ContextLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationNotifications);
    WTF_MAKE_NONCOPYABLE(ServiceWorkerRegistrationNotifications);
public:
    static ScriptPromise showNotification(ScriptState*, ServiceWorkerRegistration&, const String& title, const NotificationOptions&, ExceptionState&);
    static ScriptPromise getNotifications(ScriptState*, ServiceWorkerRegistration&, const GetNotificationOptions&);

    // ContextLifecycleObserver interface.
    void contextDestroyed() override;

    DECLARE_VIRTUAL_TRACE();

private:
    ServiceWorkerRegistrationNotifications(ExecutionContext*, ServiceWorkerRegistration*);

    static const char* supplementName();
    static ServiceWorkerRegistrationNotifications& from(ExecutionContext*, ServiceWorkerRegistration&);

    void prepareShow(const WebNotificationData&, std::unique_ptr<WebNotificationShowCallbacks>);
    void didLoadResources(PassRefPtr<SecurityOrigin>, const WebNotificationData&, std::unique_ptr<WebNotificationShowCallbacks>, NotificationResourcesLoader*);

    Member<ServiceWorkerRegistration> m_registration;
    HeapHashSet<Member<NotificationResourcesLoader>> m_loaders;
};

} // namespace blink

#endif // ServiceWorkerRegistrationNotifications_h
