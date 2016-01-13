// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_channel_host.h"

#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/java_bridge_messages.h"

using base::WaitableEvent;

namespace content {
namespace {
struct WaitableEventLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<WaitableEvent> {
  static WaitableEvent* New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    // The parenthesis is very important here to force POD type initialization.
    return new (instance) WaitableEvent(false, false);
  }
};
base::LazyInstance<WaitableEvent, WaitableEventLazyInstanceTraits> dummy_event =
    LAZY_INSTANCE_INITIALIZER;

base::subtle::AtomicWord g_last_id = 0;
}

JavaBridgeChannelHost::~JavaBridgeChannelHost() {
#if defined(OS_POSIX)
  if (channel_handle_.socket.fd > 0) {
    close(channel_handle_.socket.fd);
  }
#endif
}

JavaBridgeChannelHost* JavaBridgeChannelHost::GetJavaBridgeChannelHost(
    int renderer_id,
    base::MessageLoopProxy* ipc_message_loop) {
  std::string channel_name(base::StringPrintf("r%d.javabridge", renderer_id));
  // There's no need for a shutdown event here. If the browser is terminated
  // while the JavaBridgeChannelHost is blocked on a synchronous IPC call, the
  // renderer's shutdown event will cause the underlying channel to shut down,
  // thus terminating the IPC call.
  return static_cast<JavaBridgeChannelHost*>(NPChannelBase::GetChannel(
      channel_name,
      IPC::Channel::MODE_SERVER,
      ClassFactory,
      ipc_message_loop,
      true,
      dummy_event.Pointer()));
}

int JavaBridgeChannelHost::ThreadsafeGenerateRouteID() {
  return base::subtle::NoBarrier_AtomicIncrement(&g_last_id, 1);
}

int JavaBridgeChannelHost::GenerateRouteID() {
  return ThreadsafeGenerateRouteID();
}

bool JavaBridgeChannelHost::Init(base::MessageLoopProxy* ipc_message_loop,
                                 bool create_pipe_now,
                                 WaitableEvent* shutdown_event) {
  if (!NPChannelBase::Init(ipc_message_loop, create_pipe_now, shutdown_event)) {
    return false;
  }

  // Finish populating our ChannelHandle.
#if defined(OS_POSIX)
  // We take control of the FD for all session between this host and
  // the corresponding renderers. We keep it open until this object
  // is deleted.
  channel_handle_.socket.fd = channel_->TakeClientFileDescriptor();
#endif

  return true;
}

bool JavaBridgeChannelHost::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(JavaBridgeChannelHost, message)
    IPC_MESSAGE_HANDLER(JavaBridgeMsg_GenerateRouteID, OnGenerateRouteID)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void JavaBridgeChannelHost::OnGenerateRouteID(int* route_id) {
  *route_id = GenerateRouteID();
}

}  // namespace content
