// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/shell_client.h"

namespace shell {

ShellClient::ShellClient() {}
ShellClient::~ShellClient() {}

void ShellClient::Initialize(Connector* connector, const Identity& identity,
                             uint32_t id) {
}

InterfaceProvider* ShellClient::GetInterfaceProviderForConnection() {
  return nullptr;
}

InterfaceRegistry* ShellClient::GetInterfaceRegistryForConnection() {
  return nullptr;
}

bool ShellClient::AcceptConnection(Connection* connection) {
  return false;
}

bool ShellClient::ShellConnectionLost() { return true; }

}  // namespace shell
