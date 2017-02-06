// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for access to MIDI hardware.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/result.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MidiMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::midi::MidiPortState,
                          media::midi::MIDI_PORT_STATE_LAST)

IPC_STRUCT_TRAITS_BEGIN(media::midi::MidiPortInfo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(manufacturer)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(state)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(media::midi::Result, media::midi::Result::MAX)

// Messages for IPC between MidiMessageFilter and MidiHost.

// Renderer request to browser for access to MIDI services.
IPC_MESSAGE_CONTROL0(MidiHostMsg_StartSession)

IPC_MESSAGE_CONTROL3(MidiHostMsg_SendData,
                     uint32_t /* port */,
                     std::vector<uint8_t> /* data */,
                     double /* timestamp */)

IPC_MESSAGE_CONTROL0(MidiHostMsg_EndSession)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL1(MidiMsg_AddInputPort,
                     media::midi::MidiPortInfo /* input port */)

IPC_MESSAGE_CONTROL1(MidiMsg_AddOutputPort,
                     media::midi::MidiPortInfo /* output port */)

IPC_MESSAGE_CONTROL2(MidiMsg_SetInputPortState,
                     uint32_t /* port */,
                     media::midi::MidiPortState /* state */)

IPC_MESSAGE_CONTROL2(MidiMsg_SetOutputPortState,
                     uint32_t /* port */,
                     media::midi::MidiPortState /* state */)

IPC_MESSAGE_CONTROL1(MidiMsg_SessionStarted, media::midi::Result /* result */)

IPC_MESSAGE_CONTROL3(MidiMsg_DataReceived,
                     uint32_t /* port */,
                     std::vector<uint8_t> /* data */,
                     double /* timestamp */)

IPC_MESSAGE_CONTROL1(MidiMsg_AcknowledgeSentData, uint32_t /* bytes sent */)
