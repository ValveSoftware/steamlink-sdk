// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel.h"

namespace IPC {

// static
scoped_ptr<Channel> Channel::CreateClient(
    const IPC::ChannelHandle &channel_handle, Listener* listener) {
  return Channel::Create(channel_handle, Channel::MODE_CLIENT, listener);
}

// static
scoped_ptr<Channel> Channel::CreateNamedServer(
    const IPC::ChannelHandle &channel_handle, Listener* listener) {
  return Channel::Create(channel_handle, Channel::MODE_NAMED_SERVER, listener);
}

// static
scoped_ptr<Channel> Channel::CreateNamedClient(
    const IPC::ChannelHandle &channel_handle, Listener* listener) {
  return Channel::Create(channel_handle, Channel::MODE_NAMED_CLIENT, listener);
}

#if defined(OS_POSIX)
// static
scoped_ptr<Channel> Channel::CreateOpenNamedServer(
    const IPC::ChannelHandle &channel_handle, Listener* listener) {
  return Channel::Create(channel_handle,
                         Channel::MODE_OPEN_NAMED_SERVER,
                         listener);
}
#endif

// static
scoped_ptr<Channel> Channel::CreateServer(
    const IPC::ChannelHandle &channel_handle, Listener* listener) {
  return Channel::Create(channel_handle, Channel::MODE_SERVER, listener);
}

Channel::~Channel() {
}

}  // namespace IPC

