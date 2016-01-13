// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "mojo/embedder/platform_handle_vector.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/transport_data.h"

namespace mojo {
namespace system {

COMPILE_ASSERT(Channel::kBootstrapEndpointId !=
                   MessageInTransit::kInvalidEndpointId,
               kBootstrapEndpointId_is_invalid);

STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::EndpointId
    Channel::kBootstrapEndpointId;

Channel::EndpointInfo::EndpointInfo()
    : state(STATE_NORMAL),
      port() {
}

Channel::EndpointInfo::EndpointInfo(scoped_refptr<MessagePipe> message_pipe,
                                    unsigned port)
    : state(STATE_NORMAL),
      message_pipe(message_pipe),
      port(port) {
}

Channel::EndpointInfo::~EndpointInfo() {
}

Channel::Channel()
    : is_running_(false),
      next_local_id_(kBootstrapEndpointId) {
}

bool Channel::Init(scoped_ptr<RawChannel> raw_channel) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());
  DCHECK(raw_channel);

  // No need to take |lock_|, since this must be called before this object
  // becomes thread-safe.
  DCHECK(!is_running_no_lock());
  raw_channel_ = raw_channel.Pass();

  if (!raw_channel_->Init(this)) {
    raw_channel_.reset();
    return false;
  }

  is_running_ = true;
  return true;
}

void Channel::Shutdown() {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  IdToEndpointInfoMap to_destroy;
  {
    base::AutoLock locker(lock_);
    if (!is_running_no_lock())
      return;

    // Note: Don't reset |raw_channel_|, in case we're being called from within
    // |OnReadMessage()| or |OnFatalError()|.
    raw_channel_->Shutdown();
    is_running_ = false;

    // We need to deal with it outside the lock.
    std::swap(to_destroy, local_id_to_endpoint_info_map_);
  }

  size_t num_live = 0;
  size_t num_zombies = 0;
  for (IdToEndpointInfoMap::iterator it = to_destroy.begin();
       it != to_destroy.end();
       ++it) {
    if (it->second.state == EndpointInfo::STATE_NORMAL) {
      it->second.message_pipe->OnRemove(it->second.port);
      num_live++;
    } else {
      DCHECK(!it->second.message_pipe);
      num_zombies++;
    }
  }
  DVLOG_IF(2, num_live || num_zombies)
      << "Shut down Channel with " << num_live << " live endpoints and "
      << num_zombies << " zombies";
}

MessageInTransit::EndpointId Channel::AttachMessagePipeEndpoint(
    scoped_refptr<MessagePipe> message_pipe,
    unsigned port) {
  DCHECK(message_pipe);
  DCHECK(port == 0 || port == 1);

  MessageInTransit::EndpointId local_id;
  {
    base::AutoLock locker(lock_);

    while (next_local_id_ == MessageInTransit::kInvalidEndpointId ||
           local_id_to_endpoint_info_map_.find(next_local_id_) !=
               local_id_to_endpoint_info_map_.end())
      next_local_id_++;

    local_id = next_local_id_;
    next_local_id_++;

    // TODO(vtl): Use emplace when we move to C++11 unordered_maps. (It'll avoid
    // some expensive reference count increment/decrements.) Once this is done,
    // we should be able to delete |EndpointInfo|'s default constructor.
    local_id_to_endpoint_info_map_[local_id] = EndpointInfo(message_pipe, port);
  }

  // This might fail if that port got an |OnPeerClose()| before attaching.
  if (message_pipe->Attach(port, scoped_refptr<Channel>(this), local_id))
    return local_id;

  // Note: If it failed, quite possibly the endpoint info was removed from that
  // map (there's a race between us adding it to the map above and calling
  // |Attach()|). And even if an entry exists for |local_id|, we need to check
  // that it's the one we added (and not some other one that was added since).
  {
    base::AutoLock locker(lock_);
    IdToEndpointInfoMap::iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    if (it != local_id_to_endpoint_info_map_.end() &&
        it->second.message_pipe.get() == message_pipe.get() &&
        it->second.port == port) {
      DCHECK_EQ(it->second.state, EndpointInfo::STATE_NORMAL);
      // TODO(vtl): FIXME -- This is wrong. We need to specify (to
      // |AttachMessagePipeEndpoint()| who's going to be responsible for calling
      // |RunMessagePipeEndpoint()| ("us", or the remote by sending us a
      // |kSubtypeChannelRunMessagePipeEndpoint|). If the remote is going to
      // run, then we'll get messages to an "invalid" local ID (for running, for
      // removal).
      local_id_to_endpoint_info_map_.erase(it);
    }
  }
  return MessageInTransit::kInvalidEndpointId;
}

bool Channel::RunMessagePipeEndpoint(MessageInTransit::EndpointId local_id,
                                     MessageInTransit::EndpointId remote_id) {
  EndpointInfo endpoint_info;
  {
    base::AutoLock locker(lock_);

    IdToEndpointInfoMap::const_iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    if (it == local_id_to_endpoint_info_map_.end())
      return false;
    endpoint_info = it->second;
  }

  // Assume that this was in response to |kSubtypeChannelRunMessagePipeEndpoint|
  // and ignore it.
  if (endpoint_info.state != EndpointInfo::STATE_NORMAL) {
    DVLOG(2) << "Ignoring run message pipe endpoint for zombie endpoint "
                "(local ID " << local_id << ", remote ID " << remote_id << ")";
    return true;
  }

  // TODO(vtl): FIXME -- We need to handle the case that message pipe is already
  // running when we're here due to |kSubtypeChannelRunMessagePipeEndpoint|).
  endpoint_info.message_pipe->Run(endpoint_info.port, remote_id);
  return true;
}

void Channel::RunRemoteMessagePipeEndpoint(
    MessageInTransit::EndpointId local_id,
    MessageInTransit::EndpointId remote_id) {
#if DCHECK_IS_ON
  {
    base::AutoLock locker(lock_);
    DCHECK(local_id_to_endpoint_info_map_.find(local_id) !=
               local_id_to_endpoint_info_map_.end());
  }
#endif

  if (!SendControlMessage(
           MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint,
           local_id, remote_id)) {
    HandleLocalError(base::StringPrintf(
        "Failed to send message to run remote message pipe endpoint (local ID "
        "%u, remote ID %u)",
        static_cast<unsigned>(local_id), static_cast<unsigned>(remote_id)));
  }
}

bool Channel::WriteMessage(scoped_ptr<MessageInTransit> message) {
  base::AutoLock locker(lock_);
  if (!is_running_no_lock()) {
    // TODO(vtl): I think this is probably not an error condition, but I should
    // think about it (and the shutdown sequence) more carefully.
    LOG(WARNING) << "WriteMessage() after shutdown";
    return false;
  }

  return raw_channel_->WriteMessage(message.Pass());
}

bool Channel::IsWriteBufferEmpty() {
  base::AutoLock locker(lock_);
  if (!is_running_no_lock())
    return true;
  return raw_channel_->IsWriteBufferEmpty();
}

void Channel::DetachMessagePipeEndpoint(
    MessageInTransit::EndpointId local_id,
    MessageInTransit::EndpointId remote_id) {
  DCHECK_NE(local_id, MessageInTransit::kInvalidEndpointId);

  bool should_send_remove_message = false;
  {
    base::AutoLock locker_(lock_);
    if (!is_running_no_lock())
      return;

    IdToEndpointInfoMap::iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    DCHECK(it != local_id_to_endpoint_info_map_.end());

    switch (it->second.state) {
      case EndpointInfo::STATE_NORMAL:
        it->second.state = EndpointInfo::STATE_WAIT_REMOTE_REMOVE_ACK;
        it->second.message_pipe = NULL;
        should_send_remove_message =
            (remote_id != MessageInTransit::kInvalidEndpointId);
        break;
      case EndpointInfo::STATE_WAIT_LOCAL_DETACH:
        local_id_to_endpoint_info_map_.erase(it);
        break;
      case EndpointInfo::STATE_WAIT_REMOTE_REMOVE_ACK:
        NOTREACHED();
        break;
      case EndpointInfo::STATE_WAIT_LOCAL_DETACH_AND_REMOTE_REMOVE_ACK:
        it->second.state = EndpointInfo::STATE_WAIT_REMOTE_REMOVE_ACK;
        break;
    }
  }
  if (!should_send_remove_message)
    return;

  if (!SendControlMessage(
           MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpoint,
           local_id, remote_id)) {
    HandleLocalError(base::StringPrintf(
        "Failed to send message to remove remote message pipe endpoint (local "
        "ID %u, remote ID %u)",
        static_cast<unsigned>(local_id), static_cast<unsigned>(remote_id)));
  }
}

size_t Channel::GetSerializedPlatformHandleSize() const {
  return raw_channel_->GetSerializedPlatformHandleSize();
}

Channel::~Channel() {
  // The channel should have been shut down first.
  DCHECK(!is_running_no_lock());
}

void Channel::OnReadMessage(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  switch (message_view.type()) {
    case MessageInTransit::kTypeMessagePipeEndpoint:
    case MessageInTransit::kTypeMessagePipe:
      OnReadMessageForDownstream(message_view, platform_handles.Pass());
      break;
    case MessageInTransit::kTypeChannel:
      OnReadMessageForChannel(message_view, platform_handles.Pass());
      break;
    default:
      HandleRemoteError(base::StringPrintf(
          "Received message of invalid type %u",
          static_cast<unsigned>(message_view.type())));
      break;
  }
}

void Channel::OnFatalError(FatalError fatal_error) {
  switch (fatal_error) {
    case FATAL_ERROR_READ:
      // Most read errors aren't notable: they just reflect that the other side
      // tore down the channel.
      DVLOG(1) << "RawChannel fatal error (read)";
      break;
    case FATAL_ERROR_WRITE:
      // Write errors are slightly notable: they probably shouldn't happen under
      // normal operation (but maybe the other side crashed).
      LOG(WARNING) << "RawChannel fatal error (write)";
      break;
  }
  Shutdown();
}

void Channel::OnReadMessageForDownstream(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(message_view.type() == MessageInTransit::kTypeMessagePipeEndpoint ||
         message_view.type() == MessageInTransit::kTypeMessagePipe);

  MessageInTransit::EndpointId local_id = message_view.destination_id();
  if (local_id == MessageInTransit::kInvalidEndpointId) {
    HandleRemoteError("Received message with no destination ID");
    return;
  }

  EndpointInfo endpoint_info;
  {
    base::AutoLock locker(lock_);

    // Since we own |raw_channel_|, and this method and |Shutdown()| should only
    // be called from the creation thread, |raw_channel_| should never be null
    // here.
    DCHECK(is_running_no_lock());

    IdToEndpointInfoMap::const_iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    if (it == local_id_to_endpoint_info_map_.end()) {
      HandleRemoteError(base::StringPrintf(
          "Received a message for nonexistent local destination ID %u",
          static_cast<unsigned>(local_id)));
      // This is strongly indicative of some problem. However, it's not a fatal
      // error, since it may indicate a bug (or hostile) remote process. Don't
      // die even for Debug builds, since handling this properly needs to be
      // tested (TODO(vtl)).
      DLOG(ERROR) << "This should not happen under normal operation.";
      return;
    }
    endpoint_info = it->second;
  }

  // Ignore messages for zombie endpoints (not an error).
  if (endpoint_info.state != EndpointInfo::STATE_NORMAL) {
    DVLOG(2) << "Ignoring downstream message for zombie endpoint (local ID = "
             << local_id << ", remote ID = " << message_view.source_id() << ")";
    return;
  }

  // We need to duplicate the message (data), because |EnqueueMessage()| will
  // take ownership of it.
  scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));
  if (message_view.transport_data_buffer_size() > 0) {
    DCHECK(message_view.transport_data_buffer());
    message->SetDispatchers(
        TransportData::DeserializeDispatchers(
            message_view.transport_data_buffer(),
            message_view.transport_data_buffer_size(),
            platform_handles.Pass(),
            this));
  }
  MojoResult result = endpoint_info.message_pipe->EnqueueMessage(
      MessagePipe::GetPeerPort(endpoint_info.port), message.Pass());
  if (result != MOJO_RESULT_OK) {
    // TODO(vtl): This might be a "non-error", e.g., if the destination endpoint
    // has been closed (in an unavoidable race). This might also be a "remote"
    // error, e.g., if the remote side is sending invalid control messages (to
    // the message pipe).
    HandleLocalError(base::StringPrintf(
        "Failed to enqueue message to local ID %u (result %d)",
        static_cast<unsigned>(local_id), static_cast<int>(result)));
    return;
  }
}

void Channel::OnReadMessageForChannel(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK_EQ(message_view.type(), MessageInTransit::kTypeChannel);

  // Currently, no channel messages take platform handles.
  if (platform_handles) {
    HandleRemoteError(
        "Received invalid channel message (has platform handles)");
    NOTREACHED();
    return;
  }

  switch (message_view.subtype()) {
    case MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint:
      DVLOG(2) << "Handling channel message to run message pipe (local ID "
               << message_view.destination_id() << ", remote ID "
               << message_view.source_id() << ")";
      if (!RunMessagePipeEndpoint(message_view.destination_id(),
                                  message_view.source_id())) {
        HandleRemoteError(
            "Received invalid channel message to run message pipe");
      }
      break;
    case MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpoint:
      DVLOG(2) << "Handling channel message to remove message pipe (local ID "
               << message_view.destination_id() << ", remote ID "
               << message_view.source_id() << ")";
      if (!RemoveMessagePipeEndpoint(message_view.destination_id(),
                                     message_view.source_id())) {
        HandleRemoteError(
            "Received invalid channel message to remove message pipe");
      }
      break;
    case MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpointAck:
      DVLOG(2) << "Handling channel message to ack remove message pipe (local "
                  "ID "
               << message_view.destination_id() << ", remote ID "
               << message_view.source_id() << ")";
      if (!RemoveMessagePipeEndpoint(message_view.destination_id(),
                                     message_view.source_id())) {
        HandleRemoteError(
            "Received invalid channel message to ack remove message pipe");
      }
      break;
    default:
      HandleRemoteError("Received invalid channel message");
      NOTREACHED();
      break;
  }
}

bool Channel::RemoveMessagePipeEndpoint(
    MessageInTransit::EndpointId local_id,
    MessageInTransit::EndpointId remote_id) {
  EndpointInfo endpoint_info;
  {
    base::AutoLock locker(lock_);

    IdToEndpointInfoMap::iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    if (it == local_id_to_endpoint_info_map_.end()) {
      DVLOG(2) << "Remove message pipe error: not found";
      return false;
    }

    // If it's waiting for the remove ack, just do it and return.
    if (it->second.state == EndpointInfo::STATE_WAIT_REMOTE_REMOVE_ACK) {
      local_id_to_endpoint_info_map_.erase(it);
      return true;
    }

    if (it->second.state != EndpointInfo::STATE_NORMAL) {
      DVLOG(2) << "Remove message pipe error: wrong state";
      return false;
    }

    it->second.state = EndpointInfo::STATE_WAIT_LOCAL_DETACH;
    endpoint_info = it->second;
    it->second.message_pipe = NULL;
  }

  if (!SendControlMessage(
           MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpointAck,
           local_id, remote_id)) {
    HandleLocalError(base::StringPrintf(
        "Failed to send message to remove remote message pipe endpoint ack "
        "(local ID %u, remote ID %u)",
        static_cast<unsigned>(local_id), static_cast<unsigned>(remote_id)));
  }

  endpoint_info.message_pipe->OnRemove(endpoint_info.port);

  return true;
}

bool Channel::SendControlMessage(MessageInTransit::Subtype subtype,
                                 MessageInTransit::EndpointId local_id,
                                 MessageInTransit::EndpointId remote_id) {
  DVLOG(2) << "Sending channel control message: subtype " << subtype
           << ", local ID " << local_id << ", remote ID " << remote_id;
  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::kTypeChannel, subtype, 0, NULL));
  message->set_source_id(local_id);
  message->set_destination_id(remote_id);
  return WriteMessage(message.Pass());
}

void Channel::HandleRemoteError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this? Probably we want to
  // terminate the connection, since it's spewing invalid stuff.
  LOG(WARNING) << error_message;
}

void Channel::HandleLocalError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this?
  // Sometimes we'll want to propagate the error back to the message pipe
  // (endpoint), and notify it that the remote is (effectively) closed.
  // Sometimes we'll want to kill the channel (and notify all the endpoints that
  // their remotes are dead.
  LOG(WARNING) << error_message;
}

}  // namespace system
}  // namespace mojo
