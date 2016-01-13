// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/message_port_service.h"

#include "content/browser/message_port_message_filter.h"
#include "content/common/message_port_messages.h"

namespace content {

struct MessagePortService::MessagePort {
  // |filter| and |route_id| are what we need to send messages to the port.
  // |filter| is just a weak pointer since we get notified when its process has
  // gone away and remove it.
  MessagePortMessageFilter* filter;
  int route_id;
  // A globally unique id for this message port.
  int message_port_id;
  // The globally unique id of the entangled message port.
  int entangled_message_port_id;
  // If true, all messages to this message port are queued and not delivered.
  // This is needed so that when a message port is sent between processes all
  // pending message get transferred. There are two possibilities for pending
  // messages: either they are already received by the child process, or they're
  // in-flight. This flag ensures that the latter type get flushed through the
  // system.
  // This flag should only be set to true in response to
  // MessagePortHostMsg_QueueMessages.
  bool queue_messages;
  QueuedMessages queued_messages;
};

MessagePortService* MessagePortService::GetInstance() {
  return Singleton<MessagePortService>::get();
}

MessagePortService::MessagePortService()
    : next_message_port_id_(0) {
}

MessagePortService::~MessagePortService() {
}

void MessagePortService::UpdateMessagePort(
    int message_port_id,
    MessagePortMessageFilter* filter,
    int routing_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  port.filter = filter;
  port.route_id = routing_id;
}

void MessagePortService::OnMessagePortMessageFilterClosing(
    MessagePortMessageFilter* filter) {
  // Check if the (possibly) crashed process had any message ports.
  for (MessagePorts::iterator iter = message_ports_.begin();
       iter != message_ports_.end();) {
    MessagePorts::iterator cur_item = iter++;
    if (cur_item->second.filter == filter) {
      Erase(cur_item->first);
    }
  }
}

void MessagePortService::Create(int route_id,
                                MessagePortMessageFilter* filter,
                                int* message_port_id) {
  *message_port_id = ++next_message_port_id_;

  MessagePort port;
  port.filter = filter;
  port.route_id = route_id;
  port.message_port_id = *message_port_id;
  port.entangled_message_port_id = MSG_ROUTING_NONE;
  port.queue_messages = false;
  message_ports_[*message_port_id] = port;
}

void MessagePortService::Destroy(int message_port_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  DCHECK(message_ports_[message_port_id].queued_messages.empty());
  Erase(message_port_id);
}

void MessagePortService::Entangle(int local_message_port_id,
                                  int remote_message_port_id) {
  if (!message_ports_.count(local_message_port_id) ||
      !message_ports_.count(remote_message_port_id)) {
    NOTREACHED();
    return;
  }

  DCHECK(message_ports_[remote_message_port_id].entangled_message_port_id ==
      MSG_ROUTING_NONE);
  message_ports_[remote_message_port_id].entangled_message_port_id =
      local_message_port_id;
}

void MessagePortService::PostMessage(
    int sender_message_port_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!message_ports_.count(sender_message_port_id)) {
    NOTREACHED();
    return;
  }

  int entangled_message_port_id =
      message_ports_[sender_message_port_id].entangled_message_port_id;
  if (entangled_message_port_id == MSG_ROUTING_NONE)
    return;  // Process could have crashed.

  if (!message_ports_.count(entangled_message_port_id)) {
    NOTREACHED();
    return;
  }

  PostMessageTo(entangled_message_port_id, message, sent_message_port_ids);
}

void MessagePortService::PostMessageTo(
    int message_port_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
    if (!message_ports_.count(sent_message_port_ids[i])) {
      NOTREACHED();
      return;
    }
  }

  MessagePort& entangled_port = message_ports_[message_port_id];

  std::vector<MessagePort*> sent_ports(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i)
    sent_ports[i] = &message_ports_[sent_message_port_ids[i]];

  if (entangled_port.queue_messages) {
    entangled_port.queued_messages.push_back(
        std::make_pair(message, sent_message_port_ids));
    return;
  }

  if (!entangled_port.filter) {
    NOTREACHED();
    return;
  }

  // If a message port was sent around, the new location will need a routing
  // id.  Instead of having the created port send us a sync message to get it,
  // send along with the message.
  std::vector<int> new_routing_ids(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
    new_routing_ids[i] = entangled_port.filter->GetNextRoutingID();
    sent_ports[i]->filter = entangled_port.filter;

    // Update the entry for the sent port as it can be in a different process.
    sent_ports[i]->route_id = new_routing_ids[i];
  }

  // Now send the message to the entangled port.
  entangled_port.filter->Send(new MessagePortMsg_Message(
      entangled_port.route_id, message, sent_message_port_ids,
      new_routing_ids));
}

void MessagePortService::QueueMessages(int message_port_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  if (port.filter) {
    port.filter->Send(new MessagePortMsg_MessagesQueued(port.route_id));
    port.queue_messages = true;
    port.filter = NULL;
  }
}

void MessagePortService::SendQueuedMessages(
    int message_port_id,
    const QueuedMessages& queued_messages) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  // Send the queued messages to the port again.  This time they'll reach the
  // new location.
  MessagePort& port = message_ports_[message_port_id];
  port.queue_messages = false;
  port.queued_messages.insert(port.queued_messages.begin(),
                              queued_messages.begin(),
                              queued_messages.end());
  SendQueuedMessagesIfPossible(message_port_id);
}

void MessagePortService::SendQueuedMessagesIfPossible(int message_port_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  if (port.queue_messages || !port.filter)
    return;

  for (QueuedMessages::iterator iter = port.queued_messages.begin();
       iter != port.queued_messages.end(); ++iter) {
    PostMessageTo(message_port_id, iter->first, iter->second);
  }
  port.queued_messages.clear();
}

void MessagePortService::Erase(int message_port_id) {
  MessagePorts::iterator erase_item = message_ports_.find(message_port_id);
  DCHECK(erase_item != message_ports_.end());

  int entangled_id = erase_item->second.entangled_message_port_id;
  if (entangled_id != MSG_ROUTING_NONE) {
    // Do the disentanglement (and be paranoid about the other side existing
    // just in case something unusual happened during entanglement).
    if (message_ports_.count(entangled_id)) {
      message_ports_[entangled_id].entangled_message_port_id = MSG_ROUTING_NONE;
    }
  }
  message_ports_.erase(erase_item);
}

}  // namespace content
