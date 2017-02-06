// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

// This file defines the IPCs for the browser-side implementation of
// WebSockets.
//
// This IPC interface was originally desined based on the WebSocket
// multiplexing draft spec,
// http://tools.ietf.org/html/draft-ietf-hybi-websocket-multiplexing-09. So,
// some of them are given names correspond to the concepts defined in the spec.
//
// A WebSocketBridge object in the renderer and the corresponding WebSocketHost
// object in the browser are associated using an identifier named channel ID.
// The channel id is chosen by the renderer for a new channel. While the
// channel id is unique per-renderer, the browser may have multiple renderers
// using the same channel id.
//
// There're WebSocketDispatcherHost objects for each renderer. Each of
// WebSocketDispatcherHost holds a channel id to WebSocketHost map.
//
// Received messages are routed to the corresponding object by
// WebSocketDispatcher in the renderer and WebSocketDispatcherHost in the
// browser using the channel ID.
//
// The channel ID value is stored in the routing ID member which is available
// when we use the IPC_MESSAGE_ROUTED macro though it's unintended use.

#include <stdint.h>

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/websocket.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START WebSocketMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::WebSocketMessageType,
                          content::WEB_SOCKET_MESSAGE_TYPE_LAST)

IPC_STRUCT_BEGIN(WebSocketHostMsg_AddChannelRequest_Params)
  IPC_STRUCT_MEMBER(GURL, socket_url)
  IPC_STRUCT_MEMBER(std::vector<std::string>, requested_protocols)
  IPC_STRUCT_MEMBER(url::Origin, origin)
  IPC_STRUCT_MEMBER(GURL, first_party_for_cookies)
  IPC_STRUCT_MEMBER(std::string, user_agent_override)
  IPC_STRUCT_MEMBER(int, render_frame_id)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::WebSocketHandshakeRequest)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(headers_text)
  IPC_STRUCT_TRAITS_MEMBER(request_time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::WebSocketHandshakeResponse)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(headers_text)
  IPC_STRUCT_TRAITS_MEMBER(response_time)
IPC_STRUCT_TRAITS_END()

// WebSocket messages sent from the renderer to the browser.

// Open new WebSocket connection to |socket_url|. |requested_protocols| is a
// list of tokens identifying sub-protocols the renderer would like to use, as
// described in RFC6455 "Subprotocols Using the WebSocket Protocol".
IPC_MESSAGE_ROUTED1(WebSocketHostMsg_AddChannelRequest,
                    WebSocketHostMsg_AddChannelRequest_Params)

// Send a complete binary WebSocket message consisting of the Blob identified by
// |uuid|. The message will be split into frames as necessary. |expected_size|
// must match the browser's idea of the size of the Blob to prevent flow control
// from becoming desynchronised. If it does not match the connection will be
// terminated with a WebSocketMsg_NotifyFailure message. On success, the browser
// will have consumed |expected_size| bytes of flow control send quota and the
// renderer needs to subtract that from its running total of flow control send
// quota. See the design doc at
// https://docs.google.com/document/d/1CDiXB9pBumhFVVfmIn1CRI6v6byxyqWu2urEE9xp714/edit
// SendFrame or SendBlob IPCs must not be sent by the renderer until the
// BlobSendComplete message has been received from the browser. The renderer
// should retain a reference to the Blob until either a BlobSendComplete or
// NotifyFailure IPC is received.
IPC_MESSAGE_ROUTED2(WebSocketHostMsg_SendBlob,
                    std::string /* uuid */,
                    uint64_t /* expected_size */)

// WebSocket messages sent from the browser to the renderer.

// Respond to an AddChannelRequest. |selected_protocol| is the sub-protocol the
// server selected, or empty if no sub-protocol was selected. |extensions| is
// the list of extensions negotiated for the connection.
IPC_MESSAGE_ROUTED2(WebSocketMsg_AddChannelResponse,
                    std::string /* selected_protocol */,
                    std::string /* extensions */)

// Notify the renderer that the browser has started an opening handshake.
// This message is for showing the request in the inspector and
// can be omitted if the inspector is not active.
IPC_MESSAGE_ROUTED1(WebSocketMsg_NotifyStartOpeningHandshake,
                    content::WebSocketHandshakeRequest /* request */)

// Notify the renderer that the browser has finished an opening handshake.
// This message precedes AddChannelResponse.
// This message is for showing the response in the inspector and
// can be omitted if the inspector is not active.
IPC_MESSAGE_ROUTED1(WebSocketMsg_NotifyFinishOpeningHandshake,
                    content::WebSocketHandshakeResponse /* response */)

// Notify the renderer that either:
// - the connection open request (WebSocketHostMsg_AddChannelRequest) failed.
// - the browser is required to fail the connection
//   (see RFC6455 7.1.7 for details).
//
// When the renderer process receives this messages it does the following:
// 1. Fire an error event.
// 2. Show |message| to the inspector.
// 3. Close the channel immediately uncleanly, as if it received
//    DropChannel(was_clean = false, code = 1006, reason = "").
// |message| will be shown in the inspector and won't be passed to the script.
// TODO(yhirano): Find the way to pass |message| directly to the inspector
// process.
IPC_MESSAGE_ROUTED1(WebSocketMsg_NotifyFailure,
                    std::string /* message */)

// Indicates tbat the current Blob has finished sending. The renderer can
// release its reference on the Blob, and may now use SendFrame or SendBlob to
// send more messages.
IPC_MESSAGE_ROUTED0(WebSocketMsg_BlobSendComplete)

// WebSocket messages that can be sent in either direction.

// Send a non-control frame to the channel.
// - If the sender is the renderer, it will be sent to the remote server.
// - If the sender is the browser, it comes from the remote server.
//
// - |fin| indicates that this frame is the last in the current message.
// - |type| is the type of the message. On the first frame of a message, it
//   must be set to either WEB_SOCKET_MESSAGE_TYPE_TEXT or
//   WEB_SOCKET_MESSAGE_TYPE_BINARY. On subsequent frames, it must be set to
//   WEB_SOCKET_MESSAGE_TYPE_CONTINUATION, and the type is the same as that of
//   the first message. If |type| is WEB_SOCKET_MESSAGE_TYPE_TEXT, then the
//   concatenation of the |data| from every frame in the message must be valid
//   UTF-8. If |fin| is not set, |data| must be non-empty.
IPC_MESSAGE_ROUTED3(WebSocketMsg_SendFrame,
                    bool /* fin */,
                    content::WebSocketMessageType /* type */,
                    std::vector<char> /* data */)

// Add |quota| tokens of send quota for the channel. |quota| must be a positive
// integer. Both the browser and the renderer set send quota for the other
// side, and check that quota has not been exceeded when receiving messages.
// Both sides start a new channel with a quota of 0, and must wait for a
// FlowControl message before calling SendFrame. The total available quota on
// one side must never exceed 0x7FFFFFFFFFFFFFFF tokens.
//
// During "blob sending mode", ie. between the renderer sending a
// WebSocketHostMsg_SendBlob IPC and receiving a WebSocketMsg_BlobSendComplete
// IPC, quota is used up in the browser process to send the blob, but
// FlowControl IPCs for that quota are still sent to the renderer. The render
// process needs to take into account that quota equal to the size of the Blob
// has already been used when calculating how much send quota it has left after
// receiving BlobSendComplete.
IPC_MESSAGE_ROUTED1(WebSocketMsg_FlowControl, int64_t /* quota */)

// Drop the channel.
//
// When sent by the renderer, this will cause a Close message will be sent and
// the TCP/IP connection will be closed.
//
// When sent by the browser, this indicates that a Close has been received, the
// connection was closed, or a network or protocol error occurred.
//
// - |code| is one of the reason codes specified in RFC6455.
// - |reason|, if non-empty, is a UTF-8 encoded string which may be useful for
//   debugging but is not necessarily human-readable, as supplied by the server
//   in the Close message.
// - If |was_clean| is false on a message from the browser, then the WebSocket
//   connection was not closed cleanly. If |was_clean| is false on a message
//   from the renderer, then the connection should be closed immediately without
//   a closing handshake and the renderer cannot accept any new messages on this
//   connection.
IPC_MESSAGE_ROUTED3(WebSocketMsg_DropChannel,
                    bool /* was_clean */,
                    unsigned short /* code */,
                    std::string /* reason */)

// Notify the renderer that a closing handshake has been initiated by the
// server, so that it can set the Javascript readyState to CLOSING.
IPC_MESSAGE_ROUTED0(WebSocketMsg_NotifyClosing)
