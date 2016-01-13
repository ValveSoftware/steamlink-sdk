// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for push messaging.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START PushMessagingMsgStart

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED3(PushMessagingMsg_RegisterSuccess,
                    int32 /* callbacks_id */,
                    GURL /* endpoint */,
                    std::string /* registration_id */)

IPC_MESSAGE_ROUTED1(PushMessagingMsg_RegisterError,
                    int32 /* callbacks_id */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_CONTROL3(PushMessagingHostMsg_Register,
                     int32 /* routing_id */,
                     int32 /* callbacks_id */,
                     std::string /* sender_id */)
