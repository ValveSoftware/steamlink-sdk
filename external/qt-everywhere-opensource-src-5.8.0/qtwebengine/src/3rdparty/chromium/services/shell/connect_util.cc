// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/connect_util.h"

#include <memory>
#include <utility>

#include "services/shell/connect_params.h"
#include "services/shell/shell.h"

namespace shell {

mojo::ScopedMessagePipeHandle ConnectToInterfaceByName(
    Shell* shell,
    const Identity& source,
    const Identity& target,
    const std::string& interface_name) {
  mojom::InterfaceProviderPtr remote_interfaces;
  std::unique_ptr<ConnectParams> params(new ConnectParams);
  params->set_source(source);
  params->set_target(target);
  params->set_remote_interfaces(mojo::GetProxy(&remote_interfaces));
  shell->Connect(std::move(params));
  mojo::MessagePipe pipe;
  remote_interfaces->GetInterface(interface_name, std::move(pipe.handle1));
  return std::move(pipe.handle0);
}

}  // namespace shell
