// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_

#include "content/browser/geolocation/wifi_data_provider_common.h"

namespace content {

// Implementation of the wifi data provider for Mac OSX. Uses different API
// bindings depending on APIs detected available at runtime in order to access
// wifi scan data: Apple80211.h on OSX 10.5, CoreWLAN framework on OSX 10.6.
class MacWifiDataProvider : public WifiDataProviderCommon {
 public:
  MacWifiDataProvider();

 private:
  virtual ~MacWifiDataProvider();

  // WifiDataProviderCommon
  virtual WlanApiInterface* NewWlanApi() OVERRIDE;
  virtual WifiPollingPolicy* NewPollingPolicy() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MacWifiDataProvider);
};

// Creates and returns a new API binding for the CoreWLAN API, or NULL if the
// API can not be initialized.
WifiDataProviderCommon::WlanApiInterface* NewCoreWlanApi();

} // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_MAC_H_
