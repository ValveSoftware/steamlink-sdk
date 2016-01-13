// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device motion.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START DeviceMotionMsgStart

// Asks the browser process to activate Device Motion sensors if necessary.
IPC_MESSAGE_CONTROL0(DeviceMotionHostMsg_StartPolling)

// The browser process asynchronously returns the shared memory handle that will
// hold the data from the hardware sensors.
// See device_motion_hardware_buffer.h for a description of how
// synchronization is handled.
IPC_MESSAGE_CONTROL1(DeviceMotionMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)

// Notifies the browser process that the renderer process is not using the
// Device Motion data anymore. The number of Starts should match the number
// of Stops.
IPC_MESSAGE_CONTROL0(DeviceMotionHostMsg_StopPolling)
