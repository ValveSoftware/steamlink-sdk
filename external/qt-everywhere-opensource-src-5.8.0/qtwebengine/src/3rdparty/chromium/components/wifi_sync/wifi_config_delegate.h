// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_WIFI_CONFIG_DELEGATE_H_
#define COMPONENTS_WIFI_SYNC_WIFI_CONFIG_DELEGATE_H_

namespace wifi_sync {

class WifiCredential;

// Interface for platform-specific delegates, which provide the ability
// to configure local WiFi networks settings.
class WifiConfigDelegate {
 public:
  virtual ~WifiConfigDelegate() {}

  // Adds a local network configuration entry for a WiFi network using
  // the properties of |network_credential|. If the entry already
  // exists, the entry's passphrase is updated.
  virtual void AddToLocalNetworks(
      const WifiCredential& network_credential) = 0;
};

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_WIFI_CONFIG_DELEGATE_H_
