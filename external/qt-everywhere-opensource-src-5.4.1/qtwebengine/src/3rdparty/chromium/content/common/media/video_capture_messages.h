// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/common/common_param_traits.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/capture/video_capture_types.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START VideoCaptureMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::VideoCaptureState,
                          content::VIDEO_CAPTURE_STATE_LAST)

IPC_STRUCT_TRAITS_BEGIN(media::VideoCaptureParams)
  IPC_STRUCT_TRAITS_MEMBER(requested_format)
  IPC_STRUCT_TRAITS_MEMBER(allow_resolution_change)
IPC_STRUCT_TRAITS_END()

// TODO(nick): device_id in these messages is basically just a route_id. We
// should shift to IPC_MESSAGE_ROUTED and use MessageRouter in the filter impls.

// Notify the renderer process about the state update such as
// Start/Pause/Stop.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_StateChanged,
                     int /* device id */,
                     content::VideoCaptureState /* new state */)

// Tell the renderer process that a new buffer is allocated for video capture.
IPC_MESSAGE_CONTROL4(VideoCaptureMsg_NewBuffer,
                     int /* device id */,
                     base::SharedMemoryHandle /* handle */,
                     int /* length */,
                     int /* buffer_id */)

// Tell the renderer process that it should release a buffer previously
// allocated by VideoCaptureMsg_NewBuffer.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_FreeBuffer,
                     int /* device id */,
                     int /* buffer_id */)

// Tell the renderer process that a buffer is available from video capture.
IPC_MESSAGE_CONTROL4(VideoCaptureMsg_BufferReady,
                     int /* device id */,
                     int /* buffer_id */,
                     media::VideoCaptureFormat /* format */,
                     base::TimeTicks /* timestamp */)

// Tell the renderer process that a texture mailbox buffer is available from
// video capture.
IPC_MESSAGE_CONTROL5(VideoCaptureMsg_MailboxBufferReady,
                     int /* device_id */,
                     int /* buffer_id */,
                     gpu::MailboxHolder /* mailbox_holder */,
                     media::VideoCaptureFormat /* format */,
                     base::TimeTicks /* timestamp */)

// Notify the renderer about a device's supported formats; this is a response
// to a VideoCaptureHostMsg_GetDeviceSupportedFormats request.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_DeviceSupportedFormatsEnumerated,
                     int /* device_id */,
                     media::VideoCaptureFormats /* supported_formats */)

// Notify the renderer about a device's format(s) in use; this is a response
// to a VideoCaptureHostMsg_GetDeviceFormatInUse request.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_DeviceFormatsInUseReceived,
                     int /* device_id */,
                     media::VideoCaptureFormats /* formats_in_use */)

// Start a video capture as |device_id|, a new id picked by the renderer
// process. The session to be started is determined by |params.session_id|.
IPC_MESSAGE_CONTROL3(VideoCaptureHostMsg_Start,
                     int /* device_id */,
                     media::VideoCaptureSessionId, /* session_id */
                     media::VideoCaptureParams /* params */)

// Pause the video capture specified by |device_id|.
IPC_MESSAGE_CONTROL1(VideoCaptureHostMsg_Pause,
                     int /* device_id */)

// Close the video capture specified by |device_id|.
IPC_MESSAGE_CONTROL1(VideoCaptureHostMsg_Stop,
                     int /* device_id */)

// Tell the browser process that the renderer has finished reading from
// a buffer previously delivered by VideoCaptureMsg_BufferReady.
IPC_MESSAGE_CONTROL3(VideoCaptureHostMsg_BufferReady,
                     int /* device_id */,
                     int /* buffer_id */,
                     std::vector<uint32> /* syncpoints */)

// Get the formats supported by a device referenced by |capture_session_id|.
IPC_MESSAGE_CONTROL2(VideoCaptureHostMsg_GetDeviceSupportedFormats,
                     int /* device_id */,
                     media::VideoCaptureSessionId /* session_id */)

// Get the format(s) in use by a device referenced by |capture_session_id|.
IPC_MESSAGE_CONTROL2(VideoCaptureHostMsg_GetDeviceFormatsInUse,
                     int /* device_id */,
                     media::VideoCaptureSessionId /* session_id */)
