// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_

#include "base/macros.h"
#include "content/browser/geolocation/wifi_data_provider_common.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT WifiDataProviderWin : public WifiDataProviderCommon {
 public:
  WifiDataProviderWin();

 private:
  ~WifiDataProviderWin() override;

  // WifiDataProviderCommon
  WlanApiInterface* NewWlanApi() override;
  WifiPollingPolicy* NewPollingPolicy() override;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_
