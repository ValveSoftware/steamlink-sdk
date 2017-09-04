// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/common/client_util.h"

#include <string>

#include "base/command_line.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/service_manager/runner/common/switches.h"

namespace service_manager {

mojom::ServicePtr PassServiceRequestOnCommandLine(
    base::CommandLine* command_line, const std::string& child_token) {
  std::string token = mojo::edk::GenerateRandomToken();
  command_line->AppendSwitchASCII(switches::kPrimordialPipeToken, token);

  mojom::ServicePtr client;
  client.Bind(
      mojom::ServicePtrInfo(
          mojo::edk::CreateParentMessagePipe(token, child_token), 0));
  return client;
}

mojom::ServiceRequest GetServiceRequestFromCommandLine() {
  std::string token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPrimordialPipeToken);
  mojom::ServiceRequest request;
  if (!token.empty())
    request.Bind(mojo::edk::CreateChildMessagePipe(token));
  return request;
}

bool ServiceManagerIsRemote() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrimordialPipeToken);
}

}  // namespace service_manager
