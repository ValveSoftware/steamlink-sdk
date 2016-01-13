// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webmessageportchannel_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/child/child_process.h"
#include "content/child/child_thread.h"
#include "content/common/message_port_messages.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannelClient.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebString;

namespace content {

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    base::MessageLoopProxy* child_thread_loop)
    : client_(NULL),
      route_id_(MSG_ROUTING_NONE),
      message_port_id_(MSG_ROUTING_NONE),
      child_thread_loop_(child_thread_loop) {
  AddRef();
  Init();
}

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    int route_id,
    int message_port_id,
    base::MessageLoopProxy* child_thread_loop)
    : client_(NULL),
      route_id_(route_id),
      message_port_id_(message_port_id),
      child_thread_loop_(child_thread_loop) {
  AddRef();
  Init();
}

WebMessagePortChannelImpl::~WebMessagePortChannelImpl() {
  // If we have any queued messages with attached ports, manually destroy them.
  while (!message_queue_.empty()) {
    const std::vector<WebMessagePortChannelImpl*>& channel_array =
        message_queue_.front().ports;
    for (size_t i = 0; i < channel_array.size(); i++) {
      channel_array[i]->destroy();
    }
    message_queue_.pop();
  }

  if (message_port_id_ != MSG_ROUTING_NONE)
    Send(new MessagePortHostMsg_DestroyMessagePort(message_port_id_));

  if (route_id_ != MSG_ROUTING_NONE)
    ChildThread::current()->GetRouter()->RemoveRoute(route_id_);
}

// static
void WebMessagePortChannelImpl::CreatePair(
    base::MessageLoopProxy* child_thread_loop,
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  WebMessagePortChannelImpl* impl1 =
      new WebMessagePortChannelImpl(child_thread_loop);
  WebMessagePortChannelImpl* impl2 =
      new WebMessagePortChannelImpl(child_thread_loop);

  impl1->Entangle(impl2);
  impl2->Entangle(impl1);

  *channel1 = impl1;
  *channel2 = impl2;
}

// static
std::vector<int> WebMessagePortChannelImpl::ExtractMessagePortIDs(
    WebMessagePortChannelArray* channels) {
  std::vector<int> message_port_ids;
  if (channels) {
    message_port_ids.resize(channels->size());
    // Extract the port IDs from the source array, then free it.
    for (size_t i = 0; i < channels->size(); ++i) {
      WebMessagePortChannelImpl* webchannel =
          static_cast<WebMessagePortChannelImpl*>((*channels)[i]);
      // The message port ids might not be set up yet if this channel
      // wasn't created on the main thread.
      DCHECK(webchannel->child_thread_loop_->BelongsToCurrentThread());
      message_port_ids[i] = webchannel->message_port_id();
      webchannel->QueueMessages();
      DCHECK(message_port_ids[i] != MSG_ROUTING_NONE);
    }
    delete channels;
  }
  return message_port_ids;
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
  child_thread_loop_->ReleaseSoon(FROM_HERE, this);
}

void WebMessagePortChannelImpl::postMessage(
    const WebString& message,
    WebMessagePortChannelArray* channels) {
  if (!child_thread_loop_->BelongsToCurrentThread()) {
    child_thread_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &WebMessagePortChannelImpl::PostMessage, this, message, channels));
  } else {
    PostMessage(message, channels);
  }
}

void WebMessagePortChannelImpl::PostMessage(
    const base::string16& message,
    WebMessagePortChannelArray* channels) {
  IPC::Message* msg = new MessagePortHostMsg_PostMessage(
      message_port_id_, message, ExtractMessagePortIDs(channels));
  Send(msg);
}

bool WebMessagePortChannelImpl::tryGetMessage(
    WebString* message,
    WebMessagePortChannelArray& channels) {
  base::AutoLock auto_lock(lock_);
  if (message_queue_.empty())
    return false;

  *message = message_queue_.front().message;
  const std::vector<WebMessagePortChannelImpl*>& channel_array =
      message_queue_.front().ports;
  WebMessagePortChannelArray result_ports(channel_array.size());
  for (size_t i = 0; i < channel_array.size(); i++) {
    result_ports[i] = channel_array[i];
  }

  channels.swap(result_ports);
  message_queue_.pop();
  return true;
}

void WebMessagePortChannelImpl::Init() {
  if (!child_thread_loop_->BelongsToCurrentThread()) {
    child_thread_loop_->PostTask(
        FROM_HERE, base::Bind(&WebMessagePortChannelImpl::Init, this));
    return;
  }

  if (route_id_ == MSG_ROUTING_NONE) {
    DCHECK(message_port_id_ == MSG_ROUTING_NONE);
    Send(new MessagePortHostMsg_CreateMessagePort(
        &route_id_, &message_port_id_));
  }

  ChildThread::current()->GetRouter()->AddRoute(route_id_, this);
}

void WebMessagePortChannelImpl::Entangle(
    scoped_refptr<WebMessagePortChannelImpl> channel) {
  // The message port ids might not be set up yet, if this channel wasn't
  // created on the main thread.  So need to wait until we're on the main thread
  // before getting the other message port id.
  if (!child_thread_loop_->BelongsToCurrentThread()) {
    child_thread_loop_->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Entangle, this, channel));
    return;
  }

  Send(new MessagePortHostMsg_Entangle(
      message_port_id_, channel->message_port_id()));
}

void WebMessagePortChannelImpl::QueueMessages() {
  if (!child_thread_loop_->BelongsToCurrentThread()) {
    child_thread_loop_->PostTask(
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
  if (!child_thread_loop_->BelongsToCurrentThread()) {
    DCHECK(!message->is_sync());
    child_thread_loop_->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Send, this, message));
    return;
  }

  ChildThread::current()->GetRouter()->Send(message);
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
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  base::AutoLock auto_lock(lock_);
  Message msg;
  msg.message = message;
  if (!sent_message_port_ids.empty()) {
    msg.ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      msg.ports[i] = new WebMessagePortChannelImpl(new_routing_ids[i],
                                                   sent_message_port_ids[i],
                                                   child_thread_loop_.get());
    }
  }

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
      const std::vector<WebMessagePortChannelImpl*>& channel_array =
          message_queue_.front().ports;
      std::vector<int> port_ids(channel_array.size());
      for (size_t i = 0; i < channel_array.size(); ++i) {
        port_ids[i] = channel_array[i]->message_port_id();
        channel_array[i]->QueueMessages();
      }
      queued_messages.push_back(std::make_pair(message, port_ids));
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

WebMessagePortChannelImpl::Message::~Message() {}

}  // namespace content
