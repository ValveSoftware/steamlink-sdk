// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/java_bridge_channel.h"

#include "content/child/child_process.h"
#include "content/child/plugin_messages.h"
#include "content/common/java_bridge_messages.h"
#include "third_party/WebKit/public/web/WebBindings.h"

namespace content {

JavaBridgeChannel* JavaBridgeChannel::GetJavaBridgeChannel(
    const IPC::ChannelHandle& channel_handle,
    base::MessageLoopProxy* ipc_message_loop) {
  return static_cast<JavaBridgeChannel*>(NPChannelBase::GetChannel(
      channel_handle,
      IPC::Channel::MODE_CLIENT,
      ClassFactory,
      ipc_message_loop,
      true,
      ChildProcess::current()->GetShutDownEvent()));
}

JavaBridgeChannel::JavaBridgeChannel()
    : peer_owner_id_(new struct _NPP) {
  // Register the dummy owner Id for our peer (the Browser process) as an object
  // owner, and have all objects received from the peer owned by it.
  blink::WebBindings::registerObjectOwner(peer_owner_id_.get());
  SetDefaultNPObjectOwner(peer_owner_id_.get());
}

JavaBridgeChannel::~JavaBridgeChannel() {
  blink::WebBindings::unregisterObjectOwner(peer_owner_id_.get());
}

int JavaBridgeChannel::GenerateRouteID() {
  // Use a control message as this going to the JavaBridgeChannelHost, not an
  // injected object.
  int route_id = MSG_ROUTING_NONE;
  Send(new JavaBridgeMsg_GenerateRouteID(&route_id));
  // This should never fail, as the JavaBridgeChannelHost should always outlive
  // us.
  DCHECK_NE(MSG_ROUTING_NONE, route_id);
  return route_id;
}

bool JavaBridgeChannel::OnControlMessageReceived(const IPC::Message& msg) {
  // We need to intercept these two message types because the default
  // implementation of NPChannelBase::OnControlMessageReceived() is to
  // DCHECK(false). However, we don't need to do anything, as we don't need to
  // worry about the window system hanging when a modal dialog is displayed.
  // This is because, unlike in the case of plugins, the host does not need to
  // pump the message queue to avoid hangs.
  if (msg.type() == PluginMsg_SignalModalDialogEvent::ID ||
      msg.type() == PluginMsg_ResetModalDialogEvent::ID) {
    return true;
  }
  return NPChannelBase::OnControlMessageReceived(msg);
}

}  // namespace content
