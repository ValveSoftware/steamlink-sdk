// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For OSX 10.5 we use the system API function WirelessScanSplit. This function
// is not documented or included in the SDK, so we use a reverse-engineered
// header, osx_wifi_.h. This file is taken from the iStumbler project
// (http://www.istumbler.net).

#include "content/browser/geolocation/wifi_data_provider_mac.h"

#include <dlfcn.h>
#include <stdio.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/geolocation/osx_wifi.h"
#include "content/browser/geolocation/wifi_data_provider_common.h"

namespace content {
namespace {
// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 120000;  // 2 mins
const int kNoChangePollingInterval = 300000;  // 5 mins
const int kTwoNoChangePollingInterval = 600000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s

// Provides the wifi API binding for use when running on OSX 10.5 machines using
// the Apple80211 framework.
class Apple80211Api : public WifiDataProviderCommon::WlanApiInterface {
 public:
  Apple80211Api();
  virtual ~Apple80211Api();

  // Must be called before any other interface method. Will return false if the
  // Apple80211 framework cannot be initialized (e.g. running on post-10.5 OSX),
  // in which case no other method may be called.
  bool Init();

  // WlanApiInterface
  virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data) OVERRIDE;

 private:
  // Handle, context and function pointers for Apple80211 library.
  void* apple_80211_library_;
  WirelessContext* wifi_context_;
  WirelessAttachFunction WirelessAttach_function_;
  WirelessScanSplitFunction WirelessScanSplit_function_;
  WirelessDetachFunction WirelessDetach_function_;

  WifiData wifi_data_;

  DISALLOW_COPY_AND_ASSIGN(Apple80211Api);
};

Apple80211Api::Apple80211Api()
    : apple_80211_library_(NULL), wifi_context_(NULL),
      WirelessAttach_function_(NULL), WirelessScanSplit_function_(NULL),
      WirelessDetach_function_(NULL) {
}

Apple80211Api::~Apple80211Api() {
  if (WirelessDetach_function_)
    (*WirelessDetach_function_)(wifi_context_);
  dlclose(apple_80211_library_);
}

bool Apple80211Api::Init() {
  DVLOG(1) << "Apple80211Api::Init";
  apple_80211_library_ = dlopen(
      "/System/Library/PrivateFrameworks/Apple80211.framework/Apple80211",
      RTLD_LAZY);
  if (!apple_80211_library_) {
    DLOG(WARNING) << "Could not open Apple80211 library";
    return false;
  }
  WirelessAttach_function_ = reinterpret_cast<WirelessAttachFunction>(
      dlsym(apple_80211_library_, "WirelessAttach"));
  WirelessScanSplit_function_ = reinterpret_cast<WirelessScanSplitFunction>(
      dlsym(apple_80211_library_, "WirelessScanSplit"));
  WirelessDetach_function_ = reinterpret_cast<WirelessDetachFunction>(
      dlsym(apple_80211_library_, "WirelessDetach"));
  DCHECK(WirelessAttach_function_);
  DCHECK(WirelessScanSplit_function_);
  DCHECK(WirelessDetach_function_);

  if (!WirelessAttach_function_ || !WirelessScanSplit_function_ ||
      !WirelessDetach_function_) {
    DLOG(WARNING) << "Symbol error. Attach: " << !!WirelessAttach_function_
        << " Split: " << !!WirelessScanSplit_function_
        << " Detach: " << !!WirelessDetach_function_;
    return false;
  }

  WIErr err = (*WirelessAttach_function_)(&wifi_context_, 0);
  if (err != noErr) {
    DLOG(WARNING) << "Error attaching: " << err;
    return false;
  }
  return true;
}

bool Apple80211Api::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  DVLOG(1) << "Apple80211Api::GetAccessPointData";
  DCHECK(data);
  DCHECK(WirelessScanSplit_function_);
  CFArrayRef managed_access_points = NULL;
  CFArrayRef adhoc_access_points = NULL;
  // Arrays returned here are owned by the caller.
  WIErr err = (*WirelessScanSplit_function_)(wifi_context_,
                                             &managed_access_points,
                                             &adhoc_access_points,
                                             0);
  if (err != noErr) {
    DLOG(WARNING) << "Error spliting scan: " << err;
    return false;
  }

  if (managed_access_points == NULL) {
    DLOG(WARNING) << "managed_access_points == NULL";
    return false;
  }

  int num_access_points = CFArrayGetCount(managed_access_points);
  DVLOG(1) << "Found " << num_access_points << " managed access points";
  for (int i = 0; i < num_access_points; ++i) {
    const WirelessNetworkInfo* access_point_info =
        reinterpret_cast<const WirelessNetworkInfo*>(
        CFDataGetBytePtr(
        reinterpret_cast<const CFDataRef>(
        CFArrayGetValueAtIndex(managed_access_points, i))));

    // Currently we get only MAC address, signal strength, channel
    // signal-to-noise and SSID
    AccessPointData access_point_data;
    access_point_data.mac_address =
        MacAddressAsString16(access_point_info->macAddress);
    // WirelessNetworkInfo::signal appears to be signal strength in dBm.
    access_point_data.radio_signal_strength = access_point_info->signal;
    access_point_data.channel = access_point_info->channel;
    // WirelessNetworkInfo::noise appears to be noise floor in dBm.
    access_point_data.signal_to_noise = access_point_info->signal -
                                        access_point_info->noise;
    if (!base::UTF8ToUTF16(reinterpret_cast<const char*>(
                               access_point_info->name),
                           access_point_info->nameLen,
                           &access_point_data.ssid)) {
      access_point_data.ssid.clear();
    }
    data->insert(access_point_data);
  }

  if (managed_access_points)
    CFRelease(managed_access_points);
  if (adhoc_access_points)
    CFRelease(adhoc_access_points);

  return true;
}
}  // namespace

// static
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new MacWifiDataProvider();
}

MacWifiDataProvider::MacWifiDataProvider() {
}

MacWifiDataProvider::~MacWifiDataProvider() {
}

MacWifiDataProvider::WlanApiInterface* MacWifiDataProvider::NewWlanApi() {
  // Try and find a API binding that works: first try the officially supported
  // CoreWLAN API, and if this fails (e.g. on OSX 10.5) fall back to the reverse
  // engineered Apple80211 API.
  MacWifiDataProvider::WlanApiInterface* core_wlan_api = NewCoreWlanApi();
  if (core_wlan_api)
    return core_wlan_api;

  scoped_ptr<Apple80211Api> wlan_api(new Apple80211Api);
  if (wlan_api->Init())
    return wlan_api.release();

  DVLOG(1) << "MacWifiDataProvider : failed to initialize any wlan api";
  return NULL;
}

WifiPollingPolicy* MacWifiDataProvider::NewPollingPolicy() {
  return new GenericWifiPollingPolicy<kDefaultPollingInterval,
                                      kNoChangePollingInterval,
                                      kTwoNoChangePollingInterval,
                                      kNoWifiPollingIntervalMilliseconds>;
}

}  // namespace content
