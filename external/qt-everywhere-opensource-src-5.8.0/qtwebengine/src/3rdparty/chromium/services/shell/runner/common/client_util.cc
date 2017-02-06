// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/common/client_util.h"

#include <string>

#include "base/command_line.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/shell/runner/common/switches.h"

namespace shell {

mojom::ShellClientPtr PassShellClientRequestOnCommandLine(
    base::CommandLine* command_line, const std::string& child_token) {
  std::string token = mojo::edk::GenerateRandomToken();
  command_line->AppendSwitchASCII(switches::kPrimordialPipeToken, token);

  mojom::ShellClientPtr client;
  client.Bind(
      mojom::ShellClientPtrInfo(
          mojo::edk::CreateParentMessagePipe(token, child_token), 0));
  return client;
}

mojom::ShellClientRequest GetShellClientRequestFromCommandLine() {
  std::string token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPrimordialPipeToken);
  mojom::ShellClientRequest request;
  if (!token.empty())
    request.Bind(mojo::edk::CreateChildMessagePipe(token));
  return request;
}

bool ShellIsRemote() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrimordialPipeToken);
}

}  // namespace shell
