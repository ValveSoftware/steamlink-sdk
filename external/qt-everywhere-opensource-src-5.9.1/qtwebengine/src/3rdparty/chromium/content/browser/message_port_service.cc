// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/message_port_service.h"

#include <stddef.h>

#include "content/common/message_port_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_delegate.h"

namespace content {

struct MessagePortService::MessagePort {
  // |delegate| and |route_id| are what we need to send messages to the port.
  // |delegate| is just a raw pointer since it notifies us by calling
  // OnMessagePortDelegateClosing before it gets destroyed.
  MessagePortDelegate* delegate;
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
  bool queue_for_inflight_messages;
  // If true, all messages to this message port are queued and not delivered.
  // This is needed so that when a message port is sent to a new process all
  // messages are held in the browser process until the destination process is
  // ready to receive messages. This flag is set true when a message port is
  // transferred to a different process but there isn't immediately a
  // MessagePortDelegate available for that new process. Once the
  // destination process is ready to receive messages it sends
  // MessagePortHostMsg_ReleaseMessages to set this flag to false.
  bool hold_messages_for_destination;
  // Returns true if messages should be queued for either reason.
  bool queue_messages() const {
    return queue_for_inflight_messages || hold_messages_for_destination;
  }
  // If true, the message port should be destroyed but was currently still
  // waiting for a SendQueuedMessages message from a renderer. As soon as that
  // message is received the port will actually be destroyed.
  bool should_be_destroyed;
  QueuedMessages queued_messages;
};

MessagePortService* MessagePortService::GetInstance() {
  return base::Singleton<MessagePortService>::get();
}

MessagePortService::MessagePortService()
    : next_message_port_id_(0) {
}

MessagePortService::~MessagePortService() {
}

void MessagePortService::UpdateMessagePort(int message_port_id,
                                           MessagePortDelegate* delegate,
                                           int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  port.delegate = delegate;
  port.route_id = routing_id;
}

void MessagePortService::GetMessagePortInfo(int message_port_id,
                                            MessagePortDelegate** delegate,
                                            int* routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  const MessagePort& port = message_ports_[message_port_id];
  if (delegate)
    *delegate = port.delegate;
  if (routing_id)
    *routing_id = port.route_id;
}

void MessagePortService::OnMessagePortDelegateClosing(
    MessagePortDelegate* delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Check if the (possibly) crashed process had any message ports.
  for (MessagePorts::iterator iter = message_ports_.begin();
       iter != message_ports_.end();) {
    MessagePorts::iterator cur_item = iter++;
    if (cur_item->second.delegate == delegate) {
      Erase(cur_item->first);
    }
  }
}

void MessagePortService::Create(int route_id,
                                MessagePortDelegate* delegate,
                                int* message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  *message_port_id = ++next_message_port_id_;

  MessagePort port;
  port.delegate = delegate;
  port.route_id = route_id;
  port.message_port_id = *message_port_id;
  port.entangled_message_port_id = MSG_ROUTING_NONE;
  port.queue_for_inflight_messages = false;
  port.hold_messages_for_destination = false;
  port.should_be_destroyed = false;
  message_ports_[*message_port_id] = port;
}

void MessagePortService::Destroy(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  DCHECK(message_ports_[message_port_id].queued_messages.empty());

  Erase(message_port_id);
}

void MessagePortService::Entangle(int local_message_port_id,
                                  int remote_message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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
    const std::vector<int>& sent_message_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  PostMessageTo(entangled_message_port_id, message, sent_message_ports);
}

void MessagePortService::PostMessageTo(
    int message_port_id,
    const base::string16& message,
    const std::vector<int>& sent_message_ports) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < sent_message_ports.size(); ++i) {
    if (!message_ports_.count(sent_message_ports[i])) {
      NOTREACHED();
      return;
    }
  }

  MessagePort& entangled_port = message_ports_[message_port_id];
  if (entangled_port.queue_messages()) {
    // If the target port is currently holding messages because the destination
    // renderer isn't available yet, all message ports being sent should also be
    // put in this state.
    if (entangled_port.hold_messages_for_destination) {
      for (const auto& port : sent_message_ports)
        HoldMessages(port);
    }
    entangled_port.queued_messages.push_back(
        std::make_pair(message, sent_message_ports));
    return;
  }

  if (!entangled_port.delegate) {
    NOTREACHED();
    return;
  }

  // Now send the message to the entangled port.
  entangled_port.delegate->SendMessage(entangled_port.route_id, message,
                                       sent_message_ports);
}

void MessagePortService::QueueMessages(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  if (port.delegate) {
    port.delegate->SendMessagesAreQueued(port.route_id);
    port.queue_for_inflight_messages = true;
    port.delegate = NULL;
  }
}

void MessagePortService::SendQueuedMessages(
    int message_port_id,
    const QueuedMessages& queued_messages) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  // Send the queued messages to the port again.  This time they'll reach the
  // new location.
  MessagePort& port = message_ports_[message_port_id];
  port.queue_for_inflight_messages = false;

  // If the port is currently holding messages waiting for the target renderer,
  // all ports in messages being sent to the port should also be put on hold.
  if (port.hold_messages_for_destination) {
    for (const auto& message : queued_messages)
      for (int sent_port : message.second)
        HoldMessages(sent_port);
  }

  port.queued_messages.insert(port.queued_messages.begin(),
                              queued_messages.begin(),
                              queued_messages.end());

  if (port.should_be_destroyed)
    ClosePort(message_port_id);
  else
    SendQueuedMessagesIfPossible(message_port_id);
}

void MessagePortService::SendQueuedMessagesIfPossible(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  if (port.queue_messages() || !port.delegate)
    return;

  for (QueuedMessages::iterator iter = port.queued_messages.begin();
       iter != port.queued_messages.end(); ++iter) {
    PostMessageTo(message_port_id, iter->first, iter->second);
  }
  port.queued_messages.clear();
}

void MessagePortService::HoldMessages(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  // Any ports in messages currently in the queue should also be put on hold.
  for (const auto& message : message_ports_[message_port_id].queued_messages)
    for (int sent_port : message.second)
      HoldMessages(sent_port);

  message_ports_[message_port_id].hold_messages_for_destination = true;
}

bool MessagePortService::AreMessagesHeld(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id))
    return false;
  return message_ports_[message_port_id].hold_messages_for_destination;
}

void MessagePortService::ClosePort(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  if (message_ports_[message_port_id].queue_for_inflight_messages) {
    message_ports_[message_port_id].should_be_destroyed = true;
    return;
  }

  // First close any message ports in the queue for this message port.
  for (const auto& message : message_ports_[message_port_id].queued_messages)
    for (int sent_port : message.second)
      ClosePort(sent_port);

  Erase(message_port_id);
}

void MessagePortService::ReleaseMessages(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  message_ports_[message_port_id].hold_messages_for_destination = false;
  SendQueuedMessagesIfPossible(message_port_id);
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
