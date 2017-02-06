// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/wifi_data_provider_common.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace content {

base::string16 MacAddressAsString16(const uint8_t mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX-XX-XX-XX-XX-XX.
  static const char* const kMacFormatString =
      "%02x-%02x-%02x-%02x-%02x-%02x";
  return base::ASCIIToUTF16(base::StringPrintf(kMacFormatString,
                                               mac_as_int[0],
                                               mac_as_int[1],
                                               mac_as_int[2],
                                               mac_as_int[3],
                                               mac_as_int[4],
                                               mac_as_int[5]));
}

WifiDataProviderCommon::WifiDataProviderCommon()
    : is_first_scan_complete_(false),
      weak_factory_(this) {
}

WifiDataProviderCommon::~WifiDataProviderCommon() {
}

void WifiDataProviderCommon::StartDataProvider() {
  DCHECK(wlan_api_ == NULL);
  wlan_api_.reset(NewWlanApi());
  if (wlan_api_ == NULL) {
    // Error! Can't do scans, so don't try and schedule one.
    is_first_scan_complete_ = true;
    return;
  }

  DCHECK(polling_policy_ == NULL);
  polling_policy_.reset(NewPollingPolicy());
  DCHECK(polling_policy_ != NULL);

  // Perform first scan ASAP regardless of the polling policy. If this scan
  // fails we'll retry at a rate in line with the polling policy.
  ScheduleNextScan(0);
}

void WifiDataProviderCommon::StopDataProvider() {
  wlan_api_.reset();
  polling_policy_.reset();
}

bool WifiDataProviderCommon::GetData(WifiData* data) {
  *data = wifi_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

void WifiDataProviderCommon::DoWifiScanTask() {
  bool update_available = false;
  WifiData new_data;
  if (!wlan_api_->GetAccessPointData(&new_data.access_point_data)) {
    ScheduleNextScan(polling_policy_->NoWifiInterval());
  } else {
    update_available = wifi_data_.DiffersSignificantly(new_data);
    wifi_data_ = new_data;
    polling_policy_->UpdatePollingInterval(update_available);
    ScheduleNextScan(polling_policy_->PollingInterval());
  }
  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    RunCallbacks();
  }
}

void WifiDataProviderCommon::ScheduleNextScan(int interval) {
  client_task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&WifiDataProviderCommon::DoWifiScanTask,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(interval));
}

}  // namespace content
