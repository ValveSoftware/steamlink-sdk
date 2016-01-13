// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.
#include <vector>

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START SocketStreamMsgStart

// Web Sockets messages sent from the renderer to the browser.

// Open new Socket Stream for the |socket_url| identified by |socket_id|
// in the renderer process.
// The browser starts connecting asynchronously.
// Once Socket Stream connection is established, the browser will send
// SocketStreamMsg_Connected back.
// |render_frame_id| must be the routing id of RenderFrameImpl to which the
// Socket Stream belongs.
IPC_MESSAGE_CONTROL3(SocketStreamHostMsg_Connect,
                     int /* render_frame_id */,
                     GURL /* socket_url */,
                     int /* socket_id */)

// Request to send data on the Socket Stream.
// SocketStreamHandle can send data at most |max_pending_send_allowed| bytes,
// which is given by ViewMsg_SocketStream_Connected at any time.
// The number of pending bytes can be tracked by size of |data| sent
// and |amount_sent| parameter of ViewMsg_SocketStream_DataSent.
// That is, the following constraints is applied:
//  (accumulated total of |data|) - (accumulated total of |amount_sent|)
// <= |max_pending_send_allowed|
// If the SocketStreamHandle ever tries to exceed the
// |max_pending_send_allowed|, the connection will be closed.
IPC_MESSAGE_CONTROL2(SocketStreamHostMsg_SendData,
                     int /* socket_id */,
                     std::vector<char> /* data */)

// Request to close the Socket Stream.
// The browser will send ViewMsg_SocketStream_Closed back when the Socket
// Stream is completely closed.
IPC_MESSAGE_CONTROL1(SocketStreamHostMsg_Close,
                     int /* socket_id */)


// Speech input messages sent from the browser to the renderer.

// A |socket_id| is assigned by SocketStreamHostMsg_Connect.
// The Socket Stream is connected. The SocketStreamHandle should keep track
// of how much it has pending (how much it has requested to be sent) and
// shouldn't go over |max_pending_send_allowed| bytes.
IPC_MESSAGE_CONTROL2(SocketStreamMsg_Connected,
                     int /* socket_id */,
                     int /* max_pending_send_allowed */)

// |data| is received on the Socket Stream.
IPC_MESSAGE_CONTROL2(SocketStreamMsg_ReceivedData,
                     int /* socket_id */,
                     std::vector<char> /* data */)

// |amount_sent| bytes of data requested by
// SocketStreamHostMsg_SendData has been sent on the Socket Stream.
IPC_MESSAGE_CONTROL2(SocketStreamMsg_SentData,
                     int /* socket_id */,
                     int /* amount_sent */)

// The Socket Stream is closed.
IPC_MESSAGE_CONTROL1(SocketStreamMsg_Closed,
                     int /* socket_id */)

// The Socket Stream is failed.
IPC_MESSAGE_CONTROL2(SocketStreamMsg_Failed,
                     int /* socket_id */,
                     int /* error_code */)
