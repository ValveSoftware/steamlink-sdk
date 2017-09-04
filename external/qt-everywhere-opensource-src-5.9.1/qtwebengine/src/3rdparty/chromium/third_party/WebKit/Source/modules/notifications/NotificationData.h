// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationData_h
#define NotificationData_h

#include "modules/ModulesExport.h"
#include "public/platform/modules/notifications/WebNotificationData.h"

namespace WTF {
class String;
}

namespace blink {

class ExceptionState;
class ExecutionContext;
class NotificationOptions;

// Creates a WebNotificationData object based on the developer-provided
// notification options. An exception will be thrown on the ExceptionState when
// the given options do not match the constraints imposed by the specification.
MODULES_EXPORT WebNotificationData
createWebNotificationData(ExecutionContext*,
                          const String& title,
                          const NotificationOptions&,
                          ExceptionState&);

}  // namespace blink

#endif  // NotificationData_h
