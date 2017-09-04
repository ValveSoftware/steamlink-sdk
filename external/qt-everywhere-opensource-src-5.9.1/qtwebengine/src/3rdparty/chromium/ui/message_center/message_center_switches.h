// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_SWITCHES_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_SWITCHES_H_

#include "base/compiler_specific.h"
#include "ui/message_center/message_center_export.h"

namespace switches {

MESSAGE_CENTER_EXPORT extern const char
    kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval[];

// Flag to enable or disable notification changes while the message center
// opens.  Possible values are "" (meant default), "enabled" or "disabled".
// This flag will be removed once the feature gets stable.
MESSAGE_CENTER_EXPORT extern const char kMessageCenterChangesWhileOpen[];

}  // namespace switches

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_SWITCHES_H_
