// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/proxy_message_pipe_endpoint.h"

#include <string.h>

#include "base/logging.h"
#include "mojo/system/channel.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe_dispatcher.h"

namespace mojo {
namespace system {

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint()
    : local_id_(MessageInTransit::kInvalidEndpointId),
      remote_id_(MessageInTransit::kInvalidEndpointId),
      is_peer_open_(true) {
}

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint(
    LocalMessagePipeEndpoint* local_message_pipe_endpoint,
    bool is_peer_open)
    : local_id_(MessageInTransit::kInvalidEndpointId),
      remote_id_(MessageInTransit::kInvalidEndpointId),
      is_peer_open_(is_peer_open),
      paused_message_queue_(MessageInTransitQueue::PassContents(),
                            local_message_pipe_endpoint->message_queue()) {
  local_message_pipe_endpoint->Close();
}

ProxyMessagePipeEndpoint::~ProxyMessagePipeEndpoint() {
  DCHECK(!is_running());
  DCHECK(!is_attached());
  AssertConsistentState();
  DCHECK(paused_message_queue_.IsEmpty());
}

MessagePipeEndpoint::Type ProxyMessagePipeEndpoint::GetType() const {
  return kTypeProxy;
}

bool ProxyMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_peer_open_);

  is_peer_open_ = false;

  // If our outgoing message queue isn't empty, we shouldn't be destroyed yet.
  if (!paused_message_queue_.IsEmpty())
    return true;

  if (is_attached()) {
    if (!is_running()) {
      // If we're not running yet, we can't be destroyed yet, because we're
      // still waiting for the "run" message from the other side.
      return true;
    }

    Detach();
  }

  return false;
}

// Note: We may have to enqueue messages even when our (local) peer isn't open
// -- it may have been written to and closed immediately, before we were ready.
// This case is handled in |Run()| (which will call us).
void ProxyMessagePipeEndpoint::EnqueueMessage(
    scoped_ptr<MessageInTransit> message) {
  if (is_running()) {
    message->SerializeAndCloseDispatchers(channel_.get());

    message->set_source_id(local_id_);
    message->set_destination_id(remote_id_);
    if (!channel_->WriteMessage(message.Pass()))
      LOG(WARNING) << "Failed to write message to channel";
  } else {
    paused_message_queue_.AddMessage(message.Pass());
  }
}

void ProxyMessagePipeEndpoint::Attach(scoped_refptr<Channel> channel,
                                      MessageInTransit::EndpointId local_id) {
  DCHECK(channel);
  DCHECK_NE(local_id, MessageInTransit::kInvalidEndpointId);

  DCHECK(!is_attached());

  AssertConsistentState();
  channel_ = channel;
  local_id_ = local_id;
  AssertConsistentState();
}

bool ProxyMessagePipeEndpoint::Run(MessageInTransit::EndpointId remote_id) {
  // Assertions about arguments:
  DCHECK_NE(remote_id, MessageInTransit::kInvalidEndpointId);

  // Assertions about current state:
  DCHECK(is_attached());
  DCHECK(!is_running());

  AssertConsistentState();
  remote_id_ = remote_id;
  AssertConsistentState();

  while (!paused_message_queue_.IsEmpty())
    EnqueueMessage(paused_message_queue_.GetMessage());

  if (is_peer_open_)
    return true;  // Stay alive.

  // We were just waiting to die.
  Detach();
  return false;
}

void ProxyMessagePipeEndpoint::OnRemove() {
  Detach();
}

void ProxyMessagePipeEndpoint::Detach() {
  DCHECK(is_attached());

  AssertConsistentState();
  channel_->DetachMessagePipeEndpoint(local_id_, remote_id_);
  channel_ = NULL;
  local_id_ = MessageInTransit::kInvalidEndpointId;
  remote_id_ = MessageInTransit::kInvalidEndpointId;
  paused_message_queue_.Clear();
  AssertConsistentState();
}

#ifndef NDEBUG
void ProxyMessagePipeEndpoint::AssertConsistentState() const {
  if (is_attached()) {
    DCHECK_NE(local_id_, MessageInTransit::kInvalidEndpointId);
  } else {  // Not attached.
    DCHECK_EQ(local_id_, MessageInTransit::kInvalidEndpointId);
    DCHECK_EQ(remote_id_, MessageInTransit::kInvalidEndpointId);
  }
}
#endif

}  // namespace system
}  // namespace mojo
