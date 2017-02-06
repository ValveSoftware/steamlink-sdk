// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationManager_h
#define NotificationManager_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "public/platform/modules/notifications/notification_service.mojom-blink.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace mojom {
namespace blink {
enum class PermissionStatus;
}
}

// The notification manager, unique to the execution context, is responsible for
// connecting and communicating with the Mojo notification service.
//
// TODO(peter): Make the NotificationManager responsible for resource loading.
class NotificationManager final
    : public GarbageCollectedFinalized<NotificationManager>
    , public ContextLifecycleObserver
    , public Supplement<ExecutionContext> {
    USING_GARBAGE_COLLECTED_MIXIN(NotificationManager);
    WTF_MAKE_NONCOPYABLE(NotificationManager);
public:
    static NotificationManager* from(ExecutionContext*);
    static const char* supplementName();

    ~NotificationManager();

    // Returns the notification permission status of the current origin. This
    // method is synchronous to support the Notification.permission getter.
    mojom::blink::PermissionStatus permissionStatus() const;

    // ContextLifecycleObserver implementation.
    void contextDestroyed() override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit NotificationManager(ExecutionContext*);

    mojom::blink::NotificationServicePtr m_service;
};

} // namespace blink

#endif // NotificationManager_h
