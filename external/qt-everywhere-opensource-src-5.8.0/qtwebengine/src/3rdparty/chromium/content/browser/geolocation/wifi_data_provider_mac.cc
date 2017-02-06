// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/wifi_data_provider_mac.h"

#include "content/browser/geolocation/wifi_data_provider_common.h"
#include "content/browser/geolocation/wifi_data_provider_manager.h"

namespace content {
namespace {
// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 120000;  // 2 mins
const int kNoChangePollingInterval = 300000;  // 5 mins
const int kTwoNoChangePollingInterval = 600000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s
}  // namespace

// static
WifiDataProvider* WifiDataProviderManager::DefaultFactoryFunction() {
  return new WifiDataProviderMac();
}

WifiDataProviderMac::WifiDataProviderMac() {
}

WifiDataProviderMac::~WifiDataProviderMac() {
}

WifiDataProviderMac::WlanApiInterface* WifiDataProviderMac::NewWlanApi() {
  WifiDataProviderMac::WlanApiInterface* core_wlan_api = NewCoreWlanApi();
  if (core_wlan_api)
    return core_wlan_api;

  DVLOG(1) << "WifiDataProviderMac : failed to initialize any wlan api";
  return NULL;
}

WifiPollingPolicy* WifiDataProviderMac::NewPollingPolicy() {
  return new GenericWifiPollingPolicy<kDefaultPollingInterval,
                                      kNoChangePollingInterval,
                                      kTwoNoChangePollingInterval,
                                      kNoWifiPollingIntervalMilliseconds>;
}

}  // namespace content
