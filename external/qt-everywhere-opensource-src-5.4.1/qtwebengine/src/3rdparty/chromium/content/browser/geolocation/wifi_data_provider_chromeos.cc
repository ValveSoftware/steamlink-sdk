// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for chromeos, using proprietary APIs.

#include "content/browser/geolocation/wifi_data_provider_chromeos.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/geolocation_handler.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;  // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;  // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s

}  // namespace

WifiDataProviderChromeOs::WifiDataProviderChromeOs() : started_(false) {
}

WifiDataProviderChromeOs::~WifiDataProviderChromeOs() {
}

void WifiDataProviderChromeOs::StartDataProvider() {
  DCHECK(CalledOnClientThread());

  DCHECK(polling_policy_ == NULL);
  polling_policy_.reset(
      new GenericWifiPollingPolicy<kDefaultPollingIntervalMilliseconds,
                                   kNoChangePollingIntervalMilliseconds,
                                   kTwoNoChangePollingIntervalMilliseconds,
                                   kNoWifiPollingIntervalMilliseconds>);

  ScheduleStart();
}

void WifiDataProviderChromeOs::StopDataProvider() {
  DCHECK(CalledOnClientThread());

  polling_policy_.reset();
  ScheduleStop();
}

bool WifiDataProviderChromeOs::GetData(WifiData* data) {
  DCHECK(CalledOnClientThread());
  DCHECK(data);
  *data = wifi_data_;
  return is_first_scan_complete_;
}

void WifiDataProviderChromeOs::DoStartTaskOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DoWifiScanTaskOnUIThread();
}

void WifiDataProviderChromeOs::DoWifiScanTaskOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This method could be scheduled after a ScheduleStop.
  if (!started_)
    return;

  WifiData new_data;

  if (GetAccessPointData(&new_data.access_point_data)) {
    client_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WifiDataProviderChromeOs::DidWifiScanTask, this, new_data));
  } else {
    client_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WifiDataProviderChromeOs::DidWifiScanTaskNoResults, this));
  }
}

void WifiDataProviderChromeOs::DidWifiScanTaskNoResults() {
  DCHECK(CalledOnClientThread());
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTaskOnUIThread and this method).
  if (started_)
    ScheduleNextScan(polling_policy_->NoWifiInterval());
}

void WifiDataProviderChromeOs::DidWifiScanTask(const WifiData& new_data) {
  DCHECK(CalledOnClientThread());
  bool update_available = wifi_data_.DiffersSignificantly(new_data);
  wifi_data_ = new_data;
  // Schedule next scan if started (StopDataProvider could have been called
  // in between DoWifiScanTaskOnUIThread and this method).
  if (started_) {
    polling_policy_->UpdatePollingInterval(update_available);
    ScheduleNextScan(polling_policy_->PollingInterval());
  }

  if (update_available || !is_first_scan_complete_) {
    is_first_scan_complete_ = true;
    RunCallbacks();
  }
}

void WifiDataProviderChromeOs::ScheduleNextScan(int interval) {
  DCHECK(CalledOnClientThread());
  DCHECK(started_);
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WifiDataProviderChromeOs::DoWifiScanTaskOnUIThread, this),
      base::TimeDelta::FromMilliseconds(interval));
}

void WifiDataProviderChromeOs::ScheduleStop() {
  DCHECK(CalledOnClientThread());
  DCHECK(started_);
  started_ = false;
}

void WifiDataProviderChromeOs::ScheduleStart() {
  DCHECK(CalledOnClientThread());
  DCHECK(!started_);
  started_ = true;
  // Perform first scan ASAP regardless of the polling policy. If this scan
  // fails we'll retry at a rate in line with the polling policy.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WifiDataProviderChromeOs::DoStartTaskOnUIThread, this));
}

bool WifiDataProviderChromeOs::GetAccessPointData(
    WifiData::AccessPointDataSet* result) {
  // If wifi isn't enabled, we've effectively completed the task.
  // Return true to indicate an empty access point list.
  if (!chromeos::NetworkHandler::Get()->geolocation_handler()->wifi_enabled())
    return true;

  chromeos::WifiAccessPointVector access_points;
  int64 age_ms = 0;
  if (!chromeos::NetworkHandler::Get()->geolocation_handler()->
      GetWifiAccessPoints(&access_points, &age_ms)) {
    return false;
  }
  for (chromeos::WifiAccessPointVector::const_iterator i
           = access_points.begin();
       i != access_points.end(); ++i) {
    AccessPointData ap_data;
    ap_data.mac_address = base::ASCIIToUTF16(i->mac_address);
    ap_data.radio_signal_strength = i->signal_strength;
    ap_data.channel = i->channel;
    ap_data.signal_to_noise = i->signal_to_noise;
    ap_data.ssid = base::UTF8ToUTF16(i->ssid);
    result->insert(ap_data);
  }
  // If the age is significantly longer than our long polling time, assume the
  // data is stale and return false which will trigger a faster update.
  if (age_ms > kTwoNoChangePollingIntervalMilliseconds * 2)
    return false;
  return true;
}

// static
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new WifiDataProviderChromeOs();
}

}  // namespace content
