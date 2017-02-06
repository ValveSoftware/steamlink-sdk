// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_channel_pair.h"

#include <utility>

#include "base/logging.h"

namespace mojo {
namespace edk {

const char PlatformChannelPair::kMojoPlatformChannelHandleSwitch[] =
    "mojo-platform-channel-handle";

PlatformChannelPair::~PlatformChannelPair() {
}

ScopedPlatformHandle PlatformChannelPair::PassServerHandle() {
  return std::move(server_handle_);
}

ScopedPlatformHandle PlatformChannelPair::PassClientHandle() {
  return std::move(client_handle_);
}

void PlatformChannelPair::ChildProcessLaunched() {
  DCHECK(client_handle_.is_valid());
  client_handle_.reset();
}

}  // namespace edk
}  // namespace mojo
