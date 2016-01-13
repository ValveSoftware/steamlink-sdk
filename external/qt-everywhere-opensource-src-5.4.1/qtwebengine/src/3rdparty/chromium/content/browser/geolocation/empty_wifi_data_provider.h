// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_

#include "content/browser/geolocation/wifi_data_provider.h"

namespace content {

// An implementation of WifiDataProviderImplBase that does not provide any
// data. Used on platforms where a real implementation is not available.
class EmptyWifiDataProvider : public WifiDataProviderImplBase {
 public:
  EmptyWifiDataProvider();

  // WifiDataProviderImplBase implementation
  virtual void StartDataProvider() OVERRIDE { }
  virtual void StopDataProvider() OVERRIDE { }
  virtual bool GetData(WifiData* data) OVERRIDE;

 private:
  virtual ~EmptyWifiDataProvider();

  DISALLOW_COPY_AND_ASSIGN(EmptyWifiDataProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_EMPTY_WIFI_DATA_PROVIDER_H_
