// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/embedder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/channel.h"
#include "mojo/system/core.h"
#include "mojo/system/entrypoints.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/platform_handle_dispatcher.h"
#include "mojo/system/raw_channel.h"

namespace mojo {
namespace embedder {

// This is defined here (instead of a header file), since it's opaque to the
// outside world. But we need to define it before our (internal-only) functions
// that use it.
struct ChannelInfo {
  explicit ChannelInfo(scoped_refptr<system::Channel> channel)
      : channel(channel) {}
  ~ChannelInfo() {}

  scoped_refptr<system::Channel> channel;
};

namespace {

// Helper for |CreateChannelOnIOThread()|. (Note: May return null for some
// failures.)
scoped_refptr<system::Channel> MakeChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<system::MessagePipe> message_pipe) {
  DCHECK(platform_handle.is_valid());

  // Create and initialize a |system::Channel|.
  scoped_refptr<system::Channel> channel = new system::Channel();
  if (!channel->Init(system::RawChannel::Create(platform_handle.Pass()))) {
    // This is very unusual (e.g., maybe |platform_handle| was invalid or we
    // reached some system resource limit).
    LOG(ERROR) << "Channel::Init() failed";
    // Return null, since |Shutdown()| shouldn't be called in this case.
    return scoped_refptr<system::Channel>();
  }
  // Once |Init()| has succeeded, we have to return |channel| (since
  // |Shutdown()| will have to be called on it).

  // Attach the message pipe endpoint.
  system::MessageInTransit::EndpointId endpoint_id =
      channel->AttachMessagePipeEndpoint(message_pipe, 1);
  if (endpoint_id == system::MessageInTransit::kInvalidEndpointId) {
    // This means that, e.g., the other endpoint of the message pipe was closed
    // first. But it's not necessarily an error per se.
    DVLOG(2) << "Channel::AttachMessagePipeEndpoint() failed";
    return channel;
  }
  CHECK_EQ(endpoint_id, system::Channel::kBootstrapEndpointId);

  if (!channel->RunMessagePipeEndpoint(system::Channel::kBootstrapEndpointId,
                                       system::Channel::kBootstrapEndpointId)) {
    // Currently, there's no reason for this to fail.
    NOTREACHED() << "Channel::RunMessagePipeEndpoint() failed";
    return channel;
  }

  return channel;
}

void CreateChannelOnIOThread(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<system::MessagePipe> message_pipe,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  scoped_ptr<ChannelInfo> channel_info(
      new ChannelInfo(MakeChannel(platform_handle.Pass(), message_pipe)));

  // Hand the channel back to the embedder.
  if (callback_thread_task_runner) {
    callback_thread_task_runner->PostTask(FROM_HERE,
                                          base::Bind(callback,
                                                     channel_info.release()));
  } else {
    callback.Run(channel_info.release());
  }
}

}  // namespace

void Init() {
  system::entrypoints::SetCore(new system::Core());
}

ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(platform_handle.is_valid());

  std::pair<scoped_refptr<system::MessagePipeDispatcher>,
            scoped_refptr<system::MessagePipe> > remote_message_pipe =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe();

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(core->AddDispatcher(remote_message_pipe.first)));
  // TODO(vtl): Do we properly handle the failure case here?
  if (rv.is_valid()) {
    io_thread_task_runner->PostTask(FROM_HERE,
                                    base::Bind(&CreateChannelOnIOThread,
                                               base::Passed(&platform_handle),
                                               remote_message_pipe.second,
                                               callback,
                                               callback_thread_task_runner));
  }
  return rv.Pass();
}

void DestroyChannelOnIOThread(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  if (!channel_info->channel) {
    // Presumably, |Init()| on the channel failed.
    return;
  }

  channel_info->channel->Shutdown();
  delete channel_info;
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  DCHECK(platform_handle_wrapper_handle);

  scoped_refptr<system::Dispatcher> dispatcher(
      new system::PlatformHandleDispatcher(platform_handle.Pass()));

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  MojoHandle h = core->AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *platform_handle_wrapper_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                                     ScopedPlatformHandle* platform_handle) {
  DCHECK(platform_handle);

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  scoped_refptr<system::Dispatcher> dispatcher(
      core->GetDispatcher(platform_handle_wrapper_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (dispatcher->GetType() != system::Dispatcher::kTypePlatformHandle)
    return MOJO_RESULT_INVALID_ARGUMENT;

  *platform_handle = static_cast<system::PlatformHandleDispatcher*>(
      dispatcher.get())->PassPlatformHandle().Pass();
  return MOJO_RESULT_OK;
}

}  // namespace embedder
}  // namespace mojo
