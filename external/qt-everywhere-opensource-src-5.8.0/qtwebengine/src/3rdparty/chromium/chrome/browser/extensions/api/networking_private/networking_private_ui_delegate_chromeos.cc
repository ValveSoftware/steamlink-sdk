// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"

#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/chromeos/network/network_connect.h"

namespace chromeos {
namespace extensions {

NetworkingPrivateUIDelegateChromeOS::NetworkingPrivateUIDelegateChromeOS() {}

NetworkingPrivateUIDelegateChromeOS::~NetworkingPrivateUIDelegateChromeOS() {}

void NetworkingPrivateUIDelegateChromeOS::ShowAccountDetails(
    const std::string& guid) const {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network || network->path().empty())
    return;
  ui::NetworkConnect::Get()->ShowMobileSetup(network->path());
}

bool NetworkingPrivateUIDelegateChromeOS::HandleConnectFailed(
    const std::string& guid,
    const std::string error) const {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network || network->path().empty())
    return false;
  return ui::NetworkConnect::Get()->MaybeShowConfigureUI(network->path(),
                                                         error);
}

}  // namespace extensions
}  // namespace chromeos
