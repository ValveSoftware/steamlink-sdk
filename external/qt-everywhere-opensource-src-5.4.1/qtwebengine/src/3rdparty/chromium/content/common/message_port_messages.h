// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

// Multiply-included message file, hence no include guard.

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MessagePortMsgStart

// Singly-included section, not converted.
#ifndef CONTENT_COMMON_MESSAGE_PORT_MESSAGES_H_
#define CONTENT_COMMON_MESSAGE_PORT_MESSAGES_H_

typedef std::pair<base::string16, std::vector<int> > QueuedMessage;

#endif  // CONTENT_COMMON_MESSAGE_PORT_MESSAGES_H_

//-----------------------------------------------------------------------------
// MessagePort messages
// These are messages sent from the browser to child processes.

// Sends a message to a message port.
IPC_MESSAGE_ROUTED3(MessagePortMsg_Message,
                    base::string16 /* message */,
                    std::vector<int> /* sent_message_port_ids */,
                    std::vector<int> /* new_routing_ids */)

// Tells the Message Port Channel object that there are no more in-flight
// messages arriving.
IPC_MESSAGE_ROUTED0(MessagePortMsg_MessagesQueued)

//-----------------------------------------------------------------------------
// MessagePortHost messages
// These are messages sent from child processes to the browser.

// Creates a new Message Port Channel object.  The first paramaeter is the
// message port channel's routing id in this process.  The second parameter
// is the process-wide-unique identifier for that port.
IPC_SYNC_MESSAGE_CONTROL0_2(MessagePortHostMsg_CreateMessagePort,
                            int /* route_id */,
                            int /* message_port_id */)

// Sent when a Message Port Channel object is destroyed.
IPC_MESSAGE_CONTROL1(MessagePortHostMsg_DestroyMessagePort,
                     int /* message_port_id */)

// Sends a message to a message port.  Optionally sends a message port as
// as well if sent_message_port_id != MSG_ROUTING_NONE.
IPC_MESSAGE_CONTROL3(MessagePortHostMsg_PostMessage,
                     int /* sender_message_port_id */,
                     base::string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */)

// Causes messages sent to the remote port to be delivered to this local port.
IPC_MESSAGE_CONTROL2(MessagePortHostMsg_Entangle,
                     int /* local_message_port_id */,
                     int /* remote_message_port_id */)

// Causes the browser to queue messages sent to this port until the the port
// has made sure that all in-flight messages were routed to the new
// destination.
IPC_MESSAGE_CONTROL1(MessagePortHostMsg_QueueMessages,
                     int /* message_port_id */)

// Sends the browser all the queued messages that arrived at this message port
// after it was sent in a postMessage call.
// NOTE: MSVS can't compile the macro if std::vector<std::pair<string16, int> >
// is used, so we typedef it in worker_messages.h.
IPC_MESSAGE_CONTROL2(MessagePortHostMsg_SendQueuedMessages,
                     int /* message_port_id */,
                     std::vector<QueuedMessage> /* queued_messages */)
