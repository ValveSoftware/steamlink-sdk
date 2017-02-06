// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_channel_mojo.h"

namespace IPC {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(ChannelHandle handle, Channel::Mode mode)
      : handle_(handle), mode_(mode) {}

  std::string GetName() const override {
    return handle_.name;
  }

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    if (handle_.mojo_handle.is_valid()) {
      return ChannelMojo::Create(
          mojo::ScopedMessagePipeHandle(handle_.mojo_handle), mode_, listener);
    }
    return Channel::Create(handle_, mode_, listener);
  }

 private:
  ChannelHandle handle_;
  Channel::Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelFactory);
};

} // namespace

// static
std::unique_ptr<ChannelFactory> ChannelFactory::Create(
    const ChannelHandle& handle,
    Channel::Mode mode) {
  return base::WrapUnique(new PlatformChannelFactory(handle, mode));
}

}  // namespace IPC
