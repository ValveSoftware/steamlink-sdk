// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_

#include "base/macros.h"
#include "content/browser/geolocation/wifi_data_provider.h"

namespace content {

// An implementation of WifiDataProvider that does not provide any
// data. Used on platforms where a real implementation is not available.
class EmptyWifiDataProvider : public WifiDataProvider {
 public:
  EmptyWifiDataProvider();

  // WifiDataProvider implementation
  void StartDataProvider() override { }
  void StopDataProvider() override { }
  bool GetData(WifiData* data) override;

 private:
  ~EmptyWifiDataProvider() override;

  DISALLOW_COPY_AND_ASSIGN(EmptyWifiDataProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
