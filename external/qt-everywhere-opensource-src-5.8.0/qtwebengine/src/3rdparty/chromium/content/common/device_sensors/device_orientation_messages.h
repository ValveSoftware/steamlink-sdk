// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device orientation.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START DeviceOrientationMsgStart

// Asks the browser process to activate Device Orientation sensors if necessary.
IPC_MESSAGE_CONTROL0(DeviceOrientationHostMsg_StartPolling)

// The browser process asynchronously returns the shared memory handle that will
// hold the data from the hardware sensors.
// See device_orientation_hardware_buffer.h for a description of how
// synchronization is handled.
IPC_MESSAGE_CONTROL1(DeviceOrientationMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)

// Notifies the browser process that the renderer process is not using the
// Device Orientation data anymore. The number of Starts should match the
// number of Stops.
IPC_MESSAGE_CONTROL0(DeviceOrientationHostMsg_StopPolling)

// Same as above except the messages relate to Absolute Device Orientation,
// where orientation is provided w.r.t. a predefined coordinate frame.
IPC_MESSAGE_CONTROL0(DeviceOrientationAbsoluteHostMsg_StartPolling)
IPC_MESSAGE_CONTROL1(DeviceOrientationAbsoluteMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)
IPC_MESSAGE_CONTROL0(DeviceOrientationAbsoluteHostMsg_StopPolling)
