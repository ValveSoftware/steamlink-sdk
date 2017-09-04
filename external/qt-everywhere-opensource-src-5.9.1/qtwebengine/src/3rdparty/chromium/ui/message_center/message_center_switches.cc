// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_switches.h"

namespace switches {

// Enables message center to always move other notifications upwards when a
// notification is removed, no matter whether the message center is displayed
// top down or not.
const char kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval[] =
    "enable-message-center-always-scroll-up-upon-notification-removal";

// Flag to enable or disable notification changes while the message center
// opens. This flag will be removed once the feature gets stable.
const char kMessageCenterChangesWhileOpen[] =
    "message-center-changes-while-open";

}  // namespace switches
