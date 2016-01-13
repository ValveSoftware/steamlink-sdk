// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe_dispatcher.h"

#include "base/logging.h"
#include "mojo/system/channel.h"
#include "mojo/system/constants.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/memory.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/options_validation.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"

namespace mojo {
namespace system {

namespace {

const unsigned kInvalidPort = static_cast<unsigned>(-1);

struct SerializedMessagePipeDispatcher {
  MessageInTransit::EndpointId endpoint_id;
};

}  // namespace

// MessagePipeDispatcher -------------------------------------------------------

// static
const MojoCreateMessagePipeOptions
    MessagePipeDispatcher::kDefaultCreateOptions = {
  static_cast<uint32_t>(sizeof(MojoCreateMessagePipeOptions)),
  MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE
};

MessagePipeDispatcher::MessagePipeDispatcher(
    const MojoCreateMessagePipeOptions& /*validated_options*/)
    : port_(kInvalidPort) {
}

// static
MojoResult MessagePipeDispatcher::ValidateCreateOptions(
    const MojoCreateMessagePipeOptions* in_options,
    MojoCreateMessagePipeOptions* out_options) {
  const MojoCreateMessagePipeOptionsFlags kKnownFlags =
      MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE;

  *out_options = kDefaultCreateOptions;
  if (!in_options)
    return MOJO_RESULT_OK;

  MojoResult result =
      ValidateOptionsStructPointerSizeAndFlags<MojoCreateMessagePipeOptions>(
          in_options, kKnownFlags, out_options);
  if (result != MOJO_RESULT_OK)
    return result;

  // Checks for fields beyond |flags|:

  // (Nothing here yet.)

  return MOJO_RESULT_OK;
}

void MessagePipeDispatcher::Init(scoped_refptr<MessagePipe> message_pipe,
                                 unsigned port) {
  DCHECK(message_pipe);
  DCHECK(port == 0 || port == 1);

  message_pipe_ = message_pipe;
  port_ = port;
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return kTypeMessagePipe;
}

// static
std::pair<scoped_refptr<MessagePipeDispatcher>, scoped_refptr<MessagePipe> >
MessagePipeDispatcher::CreateRemoteMessagePipe() {
  scoped_refptr<MessagePipe> message_pipe(
      new MessagePipe(
          scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
          scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipeDispatcher> dispatcher(new MessagePipeDispatcher(
      MessagePipeDispatcher::kDefaultCreateOptions));
  dispatcher->Init(message_pipe, 0);

  return std::make_pair(dispatcher, message_pipe);
}

// static
scoped_refptr<MessagePipeDispatcher> MessagePipeDispatcher::Deserialize(
    Channel* channel,
    const void* source,
    size_t size) {
  if (size != sizeof(SerializedMessagePipeDispatcher)) {
    LOG(ERROR) << "Invalid serialized message pipe dispatcher";
    return scoped_refptr<MessagePipeDispatcher>();
  }

  std::pair<scoped_refptr<MessagePipeDispatcher>, scoped_refptr<MessagePipe> >
      remote_message_pipe = CreateRemoteMessagePipe();

  MessageInTransit::EndpointId remote_id =
      static_cast<const SerializedMessagePipeDispatcher*>(source)->endpoint_id;
  if (remote_id == MessageInTransit::kInvalidEndpointId) {
    // This means that the other end was closed, and there were no messages
    // enqueued for us.
    // TODO(vtl): This is wrong. We should produce a "dead" message pipe
    // dispatcher.
    NOTIMPLEMENTED();
    return scoped_refptr<MessagePipeDispatcher>();
  }
  MessageInTransit::EndpointId local_id =
      channel->AttachMessagePipeEndpoint(remote_message_pipe.second, 1);
  if (local_id == MessageInTransit::kInvalidEndpointId) {
    LOG(ERROR) << "Failed to deserialize message pipe dispatcher (failed to "
                  "attach; remote ID = " << remote_id << ")";
    return scoped_refptr<MessagePipeDispatcher>();
  }
  DVLOG(2) << "Deserializing message pipe dispatcher (remote ID = "
           << remote_id << ", new local ID = " << local_id << ")";

  if (!channel->RunMessagePipeEndpoint(local_id, remote_id)) {
    // In general, this shouldn't fail, since we generated |local_id| locally.
    NOTREACHED();
    return scoped_refptr<MessagePipeDispatcher>();
  }

  // TODO(vtl): FIXME -- Need some error handling here.
  channel->RunRemoteMessagePipeEndpoint(local_id, remote_id);
  return remote_message_pipe.first;
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!message_pipe_);
}

MessagePipe* MessagePipeDispatcher::GetMessagePipeNoLock() const {
  lock().AssertAcquired();
  return message_pipe_.get();
}

unsigned MessagePipeDispatcher::GetPortNoLock() const {
  lock().AssertAcquired();
  return port_;
}

void MessagePipeDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  message_pipe_->CancelAllWaiters(port_);
}

void MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  message_pipe_->Close(port_);
  message_pipe_ = NULL;
  port_ = kInvalidPort;
}

scoped_refptr<Dispatcher>
MessagePipeDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  // TODO(vtl): Currently, there are no options, so we just use
  // |kDefaultCreateOptions|. Eventually, we'll have to duplicate the options
  // too.
  scoped_refptr<MessagePipeDispatcher> rv =
      new MessagePipeDispatcher(kDefaultCreateOptions);
  rv->Init(message_pipe_, port_);
  message_pipe_ = NULL;
  port_ = kInvalidPort;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(!transports || (transports->size() > 0 &&
                         transports->size() <= kMaxMessageNumHandles));

  lock().AssertAcquired();

  if (!VerifyUserPointerWithSize<1>(bytes, num_bytes))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > kMaxMessageNumBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return message_pipe_->WriteMessage(port_, bytes, num_bytes, transports,
                                     flags);
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    void* bytes,
    uint32_t* num_bytes,
    DispatcherVector* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();

  if (num_bytes) {
    if (!VerifyUserPointer<uint32_t>(num_bytes))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (!VerifyUserPointerWithSize<1>(bytes, *num_bytes))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return message_pipe_->ReadMessage(port_, bytes, num_bytes, dispatchers,
                                    num_dispatchers, flags);
}

MojoResult MessagePipeDispatcher::AddWaiterImplNoLock(Waiter* waiter,
                                                      MojoHandleSignals signals,
                                                      uint32_t context) {
  lock().AssertAcquired();
  return message_pipe_->AddWaiter(port_, waiter, signals, context);
}

void MessagePipeDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  message_pipe_->RemoveWaiter(port_, waiter);
}

void MessagePipeDispatcher::StartSerializeImplNoLock(
    Channel* /*channel*/,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  *max_size = sizeof(SerializedMessagePipeDispatcher);
  *max_platform_handles = 0;
}

bool MessagePipeDispatcher::EndSerializeAndCloseImplNoLock(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* /*platform_handles*/) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  // Convert the local endpoint to a proxy endpoint (moving the message queue).
  message_pipe_->ConvertLocalToProxy(port_);

  // Attach the new proxy endpoint to the channel.
  MessageInTransit::EndpointId endpoint_id =
      channel->AttachMessagePipeEndpoint(message_pipe_, port_);
  // Note: It's okay to get an endpoint ID of |kInvalidEndpointId|. (It's
  // possible that the other endpoint -- the one that we're not sending -- was
  // closed in the intervening time.) In that case, we need to deserialize a
  // "dead" message pipe dispatcher on the other end. (Note that this is
  // different from just producing |MOJO_HANDLE_INVALID|.)
  DVLOG(2) << "Serializing message pipe dispatcher (local ID = " << endpoint_id
           << ")";

  // We now have a local ID. Before we can run the proxy endpoint, we need to
  // get an ack back from the other side with the remote ID.
  static_cast<SerializedMessagePipeDispatcher*>(destination)->endpoint_id =
      endpoint_id;

  message_pipe_ = NULL;
  port_ = kInvalidPort;

  *actual_size = sizeof(SerializedMessagePipeDispatcher);
  return true;
}

// MessagePipeDispatcherTransport ----------------------------------------------

MessagePipeDispatcherTransport::MessagePipeDispatcherTransport(
    DispatcherTransport transport) : DispatcherTransport(transport) {
  DCHECK_EQ(message_pipe_dispatcher()->GetType(), Dispatcher::kTypeMessagePipe);
}

}  // namespace system
}  // namespace mojo
