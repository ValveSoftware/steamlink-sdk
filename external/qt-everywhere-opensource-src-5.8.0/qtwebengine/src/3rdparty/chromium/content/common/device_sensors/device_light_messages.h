// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device light.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START DeviceLightMsgStart

// Asks the browser process to activate Device Light sensors if necessary.
IPC_MESSAGE_CONTROL0(DeviceLightHostMsg_StartPolling)

// The browser process asynchronously returns the shared memory handle that
// will hold the data from the hardware sensors.
IPC_MESSAGE_CONTROL1(DeviceLightMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)

// Notifies the browser process that the renderer process is not using the
// Device Light data anymore. The number of Starts should match the number
// of Stops.
IPC_MESSAGE_CONTROL0(DeviceLightHostMsg_StopPolling)
