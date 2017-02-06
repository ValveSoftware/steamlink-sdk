// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the media streaming.
// Multiply-included message file, hence no include guard.

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaStreamMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaStreamType,
                          content::NUM_MEDIA_TYPES - 1)

IPC_ENUM_TRAITS_MAX_VALUE(content::VideoFacingMode,
                          content::NUM_MEDIA_VIDEO_FACING_MODE - 1)

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaStreamRequestResult,
                          content::NUM_MEDIA_REQUEST_RESULTS - 1)

IPC_STRUCT_TRAITS_BEGIN(content::TrackControls)
  IPC_STRUCT_TRAITS_MEMBER(requested)
  IPC_STRUCT_TRAITS_MEMBER(stream_source)
  IPC_STRUCT_TRAITS_MEMBER(device_ids)
  IPC_STRUCT_TRAITS_MEMBER(alternate_device_ids)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::StreamControls)
  IPC_STRUCT_TRAITS_MEMBER(audio)
  IPC_STRUCT_TRAITS_MEMBER(video)
  IPC_STRUCT_TRAITS_MEMBER(hotword_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::StreamDeviceInfo)
  IPC_STRUCT_TRAITS_MEMBER(device.type)
  IPC_STRUCT_TRAITS_MEMBER(device.name)
  IPC_STRUCT_TRAITS_MEMBER(device.id)
  IPC_STRUCT_TRAITS_MEMBER(device.video_facing)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output_device_id)
  IPC_STRUCT_TRAITS_MEMBER(device.input.sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(device.input.channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(device.input.frames_per_buffer)
  IPC_STRUCT_TRAITS_MEMBER(device.input.effects)
  IPC_STRUCT_TRAITS_MEMBER(device.input.mic_positions)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.frames_per_buffer)
  IPC_STRUCT_TRAITS_MEMBER(session_id)
IPC_STRUCT_TRAITS_END()

// Message sent from the browser to the renderer

// The browser has generated a stream successfully.
IPC_MESSAGE_ROUTED4(MediaStreamMsg_StreamGenerated,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfoArray /* audio_device_list */,
                    content::StreamDeviceInfoArray /* video_device_list */)

// The browser has failed to generate a stream.
IPC_MESSAGE_ROUTED2(MediaStreamMsg_StreamGenerationFailed,
                    int /* request id */,
                    content::MediaStreamRequestResult /* result */)

// The browser reports that a media device has been stopped. Stopping was
// triggered from the browser process.
IPC_MESSAGE_ROUTED2(MediaStreamMsg_DeviceStopped,
                    std::string /* label */,
                    content::StreamDeviceInfo /* the device */)

// The browser has enumerated devices. If no devices are found
// |device_list| is empty.
// Used by Pepper and WebRTC.
IPC_MESSAGE_ROUTED2(MediaStreamMsg_DevicesEnumerated,
                    int /* request id */,
                    content::StreamDeviceInfoArray /* device_list */)

// TODO(wjia): should DeviceOpen* messages be merged with
// StreamGenerat* ones?
// The browser has opened a device successfully.
IPC_MESSAGE_ROUTED3(MediaStreamMsg_DeviceOpened,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfo /* the device */)

// The browser has failed to open a device.
IPC_MESSAGE_ROUTED1(MediaStreamMsg_DeviceOpenFailed,
                    int /* request id */)

// The browser has detected a change in the set of media devices.
IPC_MESSAGE_ROUTED0(MediaStreamMsg_DevicesChanged)

// Messages sent from the renderer to the browser.

// Request a new media stream.
IPC_MESSAGE_CONTROL5(MediaStreamHostMsg_GenerateStream,
                     int /* render frame id */,
                     int /* request id */,
                     content::StreamControls /* controls */,
                     url::Origin /* security origin */,
                     bool /* user_gesture */)

// Request to cancel the request for a new media stream.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_CancelGenerateStream,
                     int /* render frame id */,
                     int /* request id */)

// Request to close a device that has been opened by GenerateStream.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_StopStreamDevice,
                     int /* render frame id */,
                     std::string /*device_id*/)

// Request to enumerate devices.
// Used by Pepper and WebRTC.
IPC_MESSAGE_CONTROL4(MediaStreamHostMsg_EnumerateDevices,
                     int /* render frame id */,
                     int /* request id */,
                     content::MediaStreamType /* type */,
                     url::Origin /* security origin */)

// Request to stop enumerating devices.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_CancelEnumerateDevices,
                     int /* render frame id */,
                     int /* request id */)

// Request to open the device.
IPC_MESSAGE_CONTROL5(MediaStreamHostMsg_OpenDevice,
                     int /* render frame id */,
                     int /* request id */,
                     std::string /* device_id */,
                     content::MediaStreamType /* type */,
                     url::Origin /* security origin */)

// Request to close a device.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_CloseDevice,
                     int /* render frame id */,
                     std::string /*label*/)

// Subscribe to notifications about changes in the set of media devices.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_SubscribeToDeviceChangeNotifications,
                     int /* render frame id */,
                     url::Origin /* security origin */)

// Cancel notifications about changes in the set of media devices.
IPC_MESSAGE_CONTROL1(MediaStreamHostMsg_CancelDeviceChangeNotifications,
                     int /* render frame id */)

// Tell the browser process if the video capture is secure (i.e., all
// connected video sinks meet the requirement of output protection.).
// Note: the browser process only trusts the |is_sucure| value in this IPC
// message if it's comimg from a trusted, whitelisted extension. Extensions run
// in separate render processes. So it shouldn't be possible, for example, for
// a user's visit to a malicious web page to compromise a render process running
// a trusted extension to make it report falsehood in this IPC message.
IPC_MESSAGE_CONTROL3(MediaStreamHostMsg_SetCapturingLinkSecured,
                     int,                      /* session_id */
                     content::MediaStreamType, /* type */
                     bool /* is_secure */)
