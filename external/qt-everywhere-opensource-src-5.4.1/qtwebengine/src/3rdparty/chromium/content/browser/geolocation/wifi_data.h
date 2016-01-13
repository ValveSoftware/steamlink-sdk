// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_H_

#include <set>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// Wifi data relating to a single access point.
struct CONTENT_EXPORT AccessPointData {
  AccessPointData();
  ~AccessPointData();

  // MAC address, formatted as per MacAddressAsString16.
  base::string16 mac_address;
  int radio_signal_strength;  // Measured in dBm
  int channel;
  int signal_to_noise;  // Ratio in dB
  base::string16 ssid;   // Network identifier
};

// This is to allow AccessPointData to be used in std::set. We order
// lexicographically by MAC address.
struct AccessPointDataLess {
  bool operator()(const AccessPointData& data1,
                  const AccessPointData& data2) const {
    return data1.mac_address < data2.mac_address;
  }
};

// All data for wifi.
struct CONTENT_EXPORT WifiData {
  WifiData();
  ~WifiData();

  // Determines whether a new set of WiFi data differs significantly from this.
  bool DiffersSignificantly(const WifiData& other) const;

  // Store access points as a set, sorted by MAC address. This allows quick
  // comparison of sets for detecting changes and for caching.
  typedef std::set<AccessPointData, AccessPointDataLess> AccessPointDataSet;
  AccessPointDataSet access_point_data;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_H_
