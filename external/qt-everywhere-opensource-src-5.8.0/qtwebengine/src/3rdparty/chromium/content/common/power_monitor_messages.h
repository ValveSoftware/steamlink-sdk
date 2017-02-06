// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START PowerMonitorMsgStart

// Messages sent from the browser to the renderer/gpu.

// Notification of a change in power status of the computer, such
// as from switching between battery and A/C power.
IPC_MESSAGE_CONTROL1(PowerMonitorMsg_PowerStateChange,
    bool /* on_battery_power */)

// Notification that the system is suspending.
IPC_MESSAGE_CONTROL0(PowerMonitorMsg_Suspend)

// Notification that the system is resuming.
IPC_MESSAGE_CONTROL0(PowerMonitorMsg_Resume)
