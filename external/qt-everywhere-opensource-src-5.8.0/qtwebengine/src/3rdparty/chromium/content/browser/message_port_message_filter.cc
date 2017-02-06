// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/message_port_message_filter.h"

#include <stddef.h>

#include "content/browser/message_port_service.h"
#include "content/common/frame_messages.h"
#include "content/common/message_port_messages.h"

namespace content {

MessagePortMessageFilter::MessagePortMessageFilter(
    const NextRoutingIDCallback& callback)
    : BrowserMessageFilter(MessagePortMsgStart),
      next_routing_id_(callback) {
}

MessagePortMessageFilter::~MessagePortMessageFilter() {
}

void MessagePortMessageFilter::OnChannelClosing() {
  MessagePortService::GetInstance()->OnMessagePortDelegateClosing(this);
}

bool MessagePortMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MessagePortMessageFilter, message)
    IPC_MESSAGE_HANDLER(MessagePortHostMsg_CreateMessagePort,
                        OnCreateMessagePort)
    IPC_MESSAGE_FORWARD(MessagePortHostMsg_DestroyMessagePort,
                        MessagePortService::GetInstance(),
                        MessagePortService::Destroy)
    IPC_MESSAGE_FORWARD(MessagePortHostMsg_Entangle,
                        MessagePortService::GetInstance(),
                        MessagePortService::Entangle)
    IPC_MESSAGE_FORWARD(MessagePortHostMsg_PostMessage,
                        MessagePortService::GetInstance(),
                        MessagePortService::PostMessage)
    IPC_MESSAGE_FORWARD(MessagePortHostMsg_QueueMessages,
                        MessagePortService::GetInstance(),
                        MessagePortService::QueueMessages)
    IPC_MESSAGE_FORWARD(MessagePortHostMsg_SendQueuedMessages,
                        MessagePortService::GetInstance(),
                        MessagePortService::SendQueuedMessages)
    IPC_MESSAGE_FORWARD(MessagePortHostMsg_ReleaseMessages,
                        MessagePortService::GetInstance(),
                        MessagePortService::ReleaseMessages)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void MessagePortMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

int MessagePortMessageFilter::GetNextRoutingID() {
  return next_routing_id_.Run();
}

void MessagePortMessageFilter::SendMessage(
    int route_id,
    const base::string16& message,
    const std::vector<int>& sent_message_ports) {
  // Generate new routing ids for all ports that were sent around. This avoids
  // waiting for the created ports to send a sync message back to get routing
  // ids.
  std::vector<int> new_routing_ids;
  UpdateMessagePortsWithNewRoutes(sent_message_ports, &new_routing_ids);
  Send(new MessagePortMsg_Message(route_id, message, sent_message_ports,
                                  new_routing_ids));
}

void MessagePortMessageFilter::SendMessagesAreQueued(int route_id) {
  Send(new MessagePortMsg_MessagesQueued(route_id));
}

void MessagePortMessageFilter::UpdateMessagePortsWithNewRoutes(
    const std::vector<int>& message_ports,
    std::vector<int>* new_routing_ids) {
  DCHECK(new_routing_ids);
  new_routing_ids->clear();
  new_routing_ids->resize(message_ports.size());

  for (size_t i = 0; i < message_ports.size(); ++i) {
    (*new_routing_ids)[i] = GetNextRoutingID();
    MessagePortService::GetInstance()->UpdateMessagePort(
        message_ports[i],
        this,
        (*new_routing_ids)[i]);
  }
}

void MessagePortMessageFilter::RouteMessageEventWithMessagePorts(
    int routing_id,
    const FrameMsg_PostMessage_Params& params) {
  FrameMsg_PostMessage_Params new_params(params);
  UpdateMessagePortsWithNewRoutes(params.message_ports,
                                  &new_params.new_routing_ids);
  Send(new FrameMsg_PostMessageEvent(routing_id, new_params));
}

void MessagePortMessageFilter::OnCreateMessagePort(int *route_id,
                                                   int* message_port_id) {
  *route_id = next_routing_id_.Run();
  MessagePortService::GetInstance()->Create(*route_id, this, message_port_id);
}

}  // namespace content
