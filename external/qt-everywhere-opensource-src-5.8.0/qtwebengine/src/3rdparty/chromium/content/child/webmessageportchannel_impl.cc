// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webmessageportchannel_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "content/child/child_process.h"
#include "content/child/child_thread_impl.h"
#include "content/common/message_port_messages.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannelClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "v8/include/v8.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebString;

namespace content {

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner)
    : client_(NULL),
      route_id_(MSG_ROUTING_NONE),
      message_port_id_(MSG_ROUTING_NONE),
      main_thread_task_runner_(main_thread_task_runner) {
  AddRef();
  Init();
}

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    int route_id,
    int port_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner)
    : client_(NULL),
      route_id_(route_id),
      message_port_id_(port_id),
      main_thread_task_runner_(main_thread_task_runner) {
  AddRef();
  Init();
}

WebMessagePortChannelImpl::~WebMessagePortChannelImpl() {
  // If we have any queued messages with attached ports, manually destroy them.
  while (!message_queue_.empty()) {
    const WebMessagePortChannelArray& channel_array =
        message_queue_.front().ports;
    for (size_t i = 0; i < channel_array.size(); i++) {
      channel_array[i]->destroy();
    }
    message_queue_.pop();
  }

  if (message_port_id_ != MSG_ROUTING_NONE)
    Send(new MessagePortHostMsg_DestroyMessagePort(message_port_id_));

  if (route_id_ != MSG_ROUTING_NONE)
    ChildThreadImpl::current()->GetRouter()->RemoveRoute(route_id_);
}

// static
void WebMessagePortChannelImpl::CreatePair(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  WebMessagePortChannelImpl* impl1 =
      new WebMessagePortChannelImpl(main_thread_task_runner);
  WebMessagePortChannelImpl* impl2 =
      new WebMessagePortChannelImpl(main_thread_task_runner);

  impl1->Entangle(impl2);
  impl2->Entangle(impl1);

  *channel1 = impl1;
  *channel2 = impl2;
}

// static
std::vector<int>
WebMessagePortChannelImpl::ExtractMessagePortIDs(
    std::unique_ptr<WebMessagePortChannelArray> channels) {
  std::vector<int> message_ports;
  if (channels)
    message_ports = ExtractMessagePortIDs(*channels);
  return message_ports;
}

// static
std::vector<int>
WebMessagePortChannelImpl::ExtractMessagePortIDs(
    const WebMessagePortChannelArray& channels) {
  std::vector<int> message_ports(channels.size());
  for (size_t i = 0; i < channels.size(); ++i) {
    WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channels[i]);
    // The message port ids might not be set up yet if this channel
    // wasn't created on the main thread.
    DCHECK(webchannel->main_thread_task_runner_->BelongsToCurrentThread());
    message_ports[i] = webchannel->message_port_id();
    webchannel->QueueMessages();
    DCHECK(message_ports[i] != MSG_ROUTING_NONE);
  }
  return message_ports;
}

// static
std::vector<int>
WebMessagePortChannelImpl::ExtractMessagePortIDsWithoutQueueing(
    std::unique_ptr<WebMessagePortChannelArray> channels) {
  if (!channels)
    return std::vector<int>();

  std::vector<int> message_ports(channels->size());
  for (size_t i = 0; i < channels->size(); ++i) {
    WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>((*channels)[i]);
    // The message port ids might not be set up yet if this channel
    // wasn't created on the main thread.
    DCHECK(webchannel->main_thread_task_runner_->BelongsToCurrentThread());
    message_ports[i] = webchannel->message_port_id();
    // Don't queue messages, but do increase the child processes ref-count to
    // ensure this child process stays alive long enough to receive all
    // in-flight messages.
    ChildProcess::current()->AddRefProcess();
    DCHECK(message_ports[i] != MSG_ROUTING_NONE);
  }
  return message_ports;
}

// static
WebMessagePortChannelArray WebMessagePortChannelImpl::CreatePorts(
    const std::vector<int>& message_ports,
    const std::vector<int>& new_routing_ids,
    const scoped_refptr<base::SingleThreadTaskRunner>&
        main_thread_task_runner) {
  DCHECK_EQ(message_ports.size(), new_routing_ids.size());
  WebMessagePortChannelArray channels(message_ports.size());
  for (size_t i = 0; i < message_ports.size() && i < new_routing_ids.size();
       ++i) {
    channels[i] = new WebMessagePortChannelImpl(
        new_routing_ids[i], message_ports[i],
        main_thread_task_runner);
  }
  return channels;
}

void WebMessagePortChannelImpl::setClient(WebMessagePortChannelClient* client) {
  // Must lock here since client_ is called on the main thread.
  base::AutoLock auto_lock(lock_);
  client_ = client;
}

void WebMessagePortChannelImpl::destroy() {
  setClient(NULL);

  // Release the object on the main thread, since the destructor might want to
  // send an IPC, and that has to happen on the main thread.
  main_thread_task_runner_->ReleaseSoon(FROM_HERE, this);
}

void WebMessagePortChannelImpl::postMessage(
    const WebString& message,
    WebMessagePortChannelArray* channels_ptr) {
  std::unique_ptr<WebMessagePortChannelArray> channels(channels_ptr);
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebMessagePortChannelImpl::SendPostMessage, this,
                              message, base::Passed(std::move(channels))));
  } else {
    SendPostMessage(message, std::move(channels));
  }
}

void WebMessagePortChannelImpl::SendPostMessage(
    const base::string16& message,
    std::unique_ptr<WebMessagePortChannelArray> channels) {
  IPC::Message* msg = new MessagePortHostMsg_PostMessage(
      message_port_id_, message, ExtractMessagePortIDs(std::move(channels)));
  Send(msg);
}

bool WebMessagePortChannelImpl::tryGetMessage(
    WebString* message,
    WebMessagePortChannelArray& channels) {
  base::AutoLock auto_lock(lock_);
  if (message_queue_.empty())
    return false;

  *message = message_queue_.front().message;
  channels = message_queue_.front().ports;
  message_queue_.pop();
  return true;
}

void WebMessagePortChannelImpl::Init() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebMessagePortChannelImpl::Init, this));
    return;
  }

  if (route_id_ == MSG_ROUTING_NONE) {
    DCHECK(message_port_id_ == MSG_ROUTING_NONE);
    Send(new MessagePortHostMsg_CreateMessagePort(
        &route_id_, &message_port_id_));
  } else if (message_port_id_ != MSG_ROUTING_NONE) {
    Send(new MessagePortHostMsg_ReleaseMessages(message_port_id_));
  }

  ChildThreadImpl::current()->GetRouter()->AddRoute(route_id_, this);
}

void WebMessagePortChannelImpl::Entangle(
    scoped_refptr<WebMessagePortChannelImpl> channel) {
  // The message port ids might not be set up yet, if this channel wasn't
  // created on the main thread.  So need to wait until we're on the main thread
  // before getting the other message port id.
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Entangle, this, channel));
    return;
  }

  Send(new MessagePortHostMsg_Entangle(
      message_port_id_, channel->message_port_id()));
}

void WebMessagePortChannelImpl::QueueMessages() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebMessagePortChannelImpl::QueueMessages, this));
    return;
  }
  // This message port is being sent elsewhere (perhaps to another process).
  // The new endpoint needs to receive the queued messages, including ones that
  // could still be in-flight.  So we tell the browser to queue messages, and it
  // sends us an ack, whose receipt we know means that no more messages are
  // in-flight.  We then send the queued messages to the browser, which prepends
  // them to the ones it queued and it sends them to the new endpoint.
  Send(new MessagePortHostMsg_QueueMessages(message_port_id_));

  // The process could potentially go away while we're still waiting for
  // in-flight messages.  Ensure it stays alive.
  ChildProcess::current()->AddRefProcess();
}

void WebMessagePortChannelImpl::Send(IPC::Message* message) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    DCHECK(!message->is_sync());
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Send, this, message));
    return;
  }

  ChildThreadImpl::current()->GetRouter()->Send(message);
}

bool WebMessagePortChannelImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebMessagePortChannelImpl, message)
    IPC_MESSAGE_HANDLER(MessagePortMsg_Message, OnMessage)
    IPC_MESSAGE_HANDLER(MessagePortMsg_MessagesQueued, OnMessagesQueued)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebMessagePortChannelImpl::OnMessage(
    const base::string16& message,
    const std::vector<int>& sent_message_ports,
    const std::vector<int>& new_routing_ids) {
  base::AutoLock auto_lock(lock_);
  Message msg;
  msg.message = message;
  msg.ports = CreatePorts(sent_message_ports, new_routing_ids,
                          main_thread_task_runner_.get());

  bool was_empty = message_queue_.empty();
  message_queue_.push(msg);
  if (client_ && was_empty)
    client_->messageAvailable();
}

void WebMessagePortChannelImpl::OnMessagesQueued() {
  std::vector<QueuedMessage> queued_messages;

  {
    base::AutoLock auto_lock(lock_);
    queued_messages.reserve(message_queue_.size());
    while (!message_queue_.empty()) {
      base::string16 message = message_queue_.front().message;
      std::vector<int> ports =
          ExtractMessagePortIDs(message_queue_.front().ports);
      queued_messages.push_back(std::make_pair(message, ports));
      message_queue_.pop();
    }
  }

  Send(new MessagePortHostMsg_SendQueuedMessages(
      message_port_id_, queued_messages));

  message_port_id_ = MSG_ROUTING_NONE;

  Release();
  ChildProcess::current()->ReleaseProcess();
}

WebMessagePortChannelImpl::Message::Message() {}

WebMessagePortChannelImpl::Message::Message(const Message& other) = default;

WebMessagePortChannelImpl::Message::~Message() {}

}  // namespace content
