// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/empty_wifi_data_provider.h"

#include "content/browser/geolocation/wifi_data_provider_manager.h"

namespace content {

EmptyWifiDataProvider::EmptyWifiDataProvider() {
}

EmptyWifiDataProvider::~EmptyWifiDataProvider() {
}

bool EmptyWifiDataProvider::GetData(WifiData* data) {
  DCHECK(data);
  // This is all the data we can get - nothing.
  return true;
}

// static
WifiDataProvider* WifiDataProviderManager::DefaultFactoryFunction() {
  return new EmptyWifiDataProvider();
}

}  // namespace content
