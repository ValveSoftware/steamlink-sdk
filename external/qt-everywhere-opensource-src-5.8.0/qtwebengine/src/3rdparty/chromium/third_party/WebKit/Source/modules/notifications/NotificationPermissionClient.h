// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationPermissionClient_h
#define NotificationPermissionClient_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class NotificationPermissionCallback;
class ScriptState;

class NotificationPermissionClient : public Supplement<LocalFrame> {
public:
    virtual ~NotificationPermissionClient() { }

    // Requests user permission to show platform notifications from the origin of the
    // current frame. The provided callback will be ran when the user has made a decision.
    virtual ScriptPromise requestPermission(ScriptState*, NotificationPermissionCallback*) = 0;

    // Supplement requirements.
    static const char* supplementName();
    static NotificationPermissionClient* from(ExecutionContext*);
};

MODULES_EXPORT void provideNotificationPermissionClientTo(LocalFrame&, NotificationPermissionClient*);

} // namespace blink

#endif // NotificationPermissionClient_h
