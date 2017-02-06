// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationPermissionClientImpl_h
#define NotificationPermissionClientImpl_h

#include "modules/notifications/NotificationPermissionClient.h"
#include "platform/heap/Handle.h"

namespace blink {

class NotificationPermissionClientImpl : public GarbageCollectedFinalized<NotificationPermissionClientImpl>, public NotificationPermissionClient {
    USING_GARBAGE_COLLECTED_MIXIN(NotificationPermissionClientImpl);
public:
    static NotificationPermissionClientImpl* create();

    ~NotificationPermissionClientImpl() override;

    // NotificationPermissionClient implementation.
    ScriptPromise requestPermission(ScriptState*, NotificationPermissionCallback*) override;

    // GarbageCollectedFinalized implementation.
    DEFINE_INLINE_VIRTUAL_TRACE() { NotificationPermissionClient::trace(visitor); }

private:
    NotificationPermissionClientImpl();
};

} // namespace blink

#endif // NotificationPermissionClientImpl_h
