// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the audio.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include <string>

#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/gpu/ipc/common/media_param_traits.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START AudioMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioInputIPCDelegateState,
                          media::AUDIO_INPUT_IPC_DELEGATE_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioOutputIPCDelegateState,
                          media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::OutputDeviceStatus,
                          media::OUTPUT_DEVICE_STATUS_LAST)

IPC_STRUCT_BEGIN(AudioInputHostMsg_CreateStream_Config)
  IPC_STRUCT_MEMBER(media::AudioParameters, params)
  IPC_STRUCT_MEMBER(bool, automatic_gain_control)
  IPC_STRUCT_MEMBER(uint32_t, shared_memory_count)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Tell the renderer process that an audio output device has been authorized
// for a given stream. The renderer is given the output parameters for the
// authorized device.
IPC_MESSAGE_CONTROL4(AudioMsg_NotifyDeviceAuthorized,
                     int /* stream id */,
                     media::OutputDeviceStatus /* device_status */,
                     media::AudioParameters /* output parameters */,
                     std::string /* matched_device_id */)

// Tell the renderer process that an audio stream has been created.
// The renderer process is given a shared memory handle for the audio data
// buffer it shares with the browser process. It is also given a SyncSocket that
// it uses to communicate with the browser process about the state of the
// buffered audio data.
IPC_MESSAGE_CONTROL4(
    AudioMsg_NotifyStreamCreated,
    int /* stream id */,
    base::SharedMemoryHandle /* handle */,
    base::SyncSocket::TransitDescriptor /* socket descriptor */,
    uint32_t /* length */)

// Tell the renderer process that an audio input stream has been created.
// The renderer process would be given a SyncSocket that it should read
// from from then on. It is also given number of segments in shared memory.
IPC_MESSAGE_CONTROL5(
    AudioInputMsg_NotifyStreamCreated,
    int /* stream id */,
    base::SharedMemoryHandle /* handle */,
    base::SyncSocket::TransitDescriptor /* socket descriptor */,
    uint32_t /* length */,
    uint32_t /* segment count */)

// Notification message sent from AudioRendererHost to renderer for state
// update after the renderer has requested a Create/Start/Close.
IPC_MESSAGE_CONTROL2(AudioMsg_NotifyStreamStateChanged,
                     int /* stream id */,
                     media::AudioOutputIPCDelegateState /* new state */)

// Notification message sent from browser to renderer for state update.
IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyStreamStateChanged,
                     int /* stream id */,
                     media::AudioInputIPCDelegateState /* new state */)

IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyStreamVolume,
                     int /* stream id */,
                     double /* volume */)

// Messages sent from the renderer to the browser.

// Message sent to the browser to request the use of an audio output
// device. |render_frame_id| is the routing ID for the RenderFrame producing
// the audio data.
IPC_MESSAGE_CONTROL5(AudioHostMsg_RequestDeviceAuthorization,
                     int /* stream_id */,
                     int /* render_frame_id */,
                     int /* session_id */,
                     std::string /* device_id */,
                     url::Origin /* security_origin */)

// Request that is sent to the browser for creating an audio output stream.
IPC_MESSAGE_CONTROL3(AudioHostMsg_CreateStream,
                     int /* stream_id */,
                     int /* render_frame_id */,
                     media::AudioParameters /* params */)

// Request that is sent to the browser for creating an audio input stream.
// |render_frame_id| is the routing ID for the RenderFrame consuming the audio
// data.
IPC_MESSAGE_CONTROL4(AudioInputHostMsg_CreateStream,
                     int /* stream_id */,
                     int /* render_frame_id */,
                     int /* session_id */,
                     AudioInputHostMsg_CreateStream_Config)

// Start buffering and play the audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_PlayStream,
                     int /* stream_id */)

// Start recording the audio input stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_RecordStream,
                     int /* stream_id */)

// Pause the audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_PauseStream,
                     int /* stream_id */)

// Close an audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_CloseStream,
                     int /* stream_id */)

// Close an audio input stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_CloseStream,
                     int /* stream_id */)

// Set audio volume of the stream specified by stream_id.
// TODO(hclam): change this to vector if we have channel numbers other than 2.
IPC_MESSAGE_CONTROL2(AudioHostMsg_SetVolume,
                     int /* stream_id */,
                     double /* volume */)

// Set audio volume of the input stream specified by stream_id.
IPC_MESSAGE_CONTROL2(AudioInputHostMsg_SetVolume,
                     int /* stream_id */,
                     double /* volume */)
