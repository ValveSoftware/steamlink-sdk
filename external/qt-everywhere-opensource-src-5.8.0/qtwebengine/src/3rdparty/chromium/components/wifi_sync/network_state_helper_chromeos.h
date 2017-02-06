// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_NETWORK_STATE_HELPER_CHROMEOS_H_
#define COMPONENTS_WIFI_SYNC_NETWORK_STATE_HELPER_CHROMEOS_H_

#include <string>

#include "components/wifi_sync/wifi_credential.h"

namespace chromeos {
class NetworkStateHandler;
}

namespace wifi_sync {

// Returns a platform-agnostic representation of the ChromeOS network
// configuration state. The configuration state is filtered, so that
// only items related to ChromeOS |shill_profile_path| are returned.
WifiCredential::CredentialSet GetWifiCredentialsForShillProfile(
    chromeos::NetworkStateHandler* network_state_handler,
    const std::string& shill_profile_path);

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_NETWORK_STATE_HELPER_CHROMEOS_H_
