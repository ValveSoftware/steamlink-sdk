// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for injected Java objects. See JavaBridgeDispatcher for details.

// Multiply-included message file, hence no include guard.

#if defined(OS_ANDROID)

#include "content/child/plugin_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START JavaBridgeMsgStart

// Messages for handling Java objects injected into JavaScript -----------------

// Sent from browser to renderer to add a Java object with the given name.
IPC_MESSAGE_ROUTED2(JavaBridgeMsg_AddNamedObject,
                    base::string16 /* name */,
                    content::NPVariant_Param) /* object */

// Sent from browser to renderer to remove a Java object with the given name.
IPC_MESSAGE_ROUTED1(JavaBridgeMsg_RemoveNamedObject,
                    base::string16 /* name */)

// Sent from renderer to browser to request a route ID for a renderer-side (ie
// JavaScript) object.
IPC_SYNC_MESSAGE_CONTROL0_1(JavaBridgeMsg_GenerateRouteID,
                            int /* route_id */)

// Sent from renderer to browser to get the channel handle for NP channel.
IPC_SYNC_MESSAGE_ROUTED0_1(JavaBridgeHostMsg_GetChannelHandle,
                           IPC::ChannelHandle) /* channel handle */

#endif  // defined(OS_ANDROID)
