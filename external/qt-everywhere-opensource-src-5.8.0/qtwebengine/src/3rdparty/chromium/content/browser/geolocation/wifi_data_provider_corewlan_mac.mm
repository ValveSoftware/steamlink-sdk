// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements a WLAN API binding for CoreWLAN, as available on OSX 10.6

#include "content/browser/geolocation/wifi_data_provider_mac.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>
#include <stdint.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/sys_string_conversions.h"

// Define a subset of the CoreWLAN interfaces we require. We can't depend on
// CoreWLAN.h existing as we need to build on 10.5 SDKs. We can't just send
// messages to an untyped id due to the build treating warnings as errors,
// hence the reason we need class definitions.
// TODO(joth): When we build all 10.6 code exclusively 10.6 SDK (or later)
// tidy this up to use the framework directly. See http://crbug.com/37703

@interface CWInterface : NSObject
+ (CWInterface*)interface;
+ (CWInterface*)interfaceWithName:(NSString*)name;
+ (NSArray*)supportedInterfaces;
- (NSArray*)scanForNetworksWithParameters:(NSDictionary*)parameters
                                    error:(NSError**)error;
@end

@interface CWNetwork : NSObject <NSCopying, NSCoding>
@property (nonatomic, readonly) NSString* ssid;
@property (nonatomic, readonly) NSString* bssid;
@property (nonatomic, readonly) NSData* bssidData;
@property (nonatomic, readonly) NSNumber* securityMode;
@property (nonatomic, readonly) NSNumber* phyMode;
@property (nonatomic, readonly) NSNumber* channel;
@property (nonatomic, readonly) NSNumber* rssi;
@property (nonatomic, readonly) NSInteger rssiValue;
@property (nonatomic, readonly) NSNumber* noise;
@property (nonatomic, readonly) NSData* ieData;
@property (nonatomic, readonly) BOOL isIBSS;
- (BOOL)isEqualToNetwork:(CWNetwork*)network;
@end

namespace content {

class CoreWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  CoreWlanApi() {}

  // Must be called before any other interface method. Will return false if the
  // CoreWLAN framework cannot be initialized (e.g. running on pre-10.6 OSX),
  // in which case no other method may be called.
  bool Init();

  // WlanApiInterface
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  base::scoped_nsobject<NSBundle> bundle_;
  base::scoped_nsobject<NSString> merge_key_;

  DISALLOW_COPY_AND_ASSIGN(CoreWlanApi);
};

bool CoreWlanApi::Init() {
  // As the WLAN api binding runs on its own thread, we need to provide our own
  // auto release pool. It's simplest to do this as an automatic variable in
  // each method that needs it, to ensure the scoping is correct and does not
  // interfere with any other code using autorelease pools on the thread.
  base::mac::ScopedNSAutoreleasePool auto_pool;
  bundle_.reset([[NSBundle alloc]
      initWithPath:@"/System/Library/Frameworks/CoreWLAN.framework"]);
  if (!bundle_) {
    DVLOG(1) << "Failed to load the CoreWLAN framework bundle";
    return false;
  }

  // Dynamically look up the value of the kCWScanKeyMerge (i.e. without build
  // time dependency on the 10.6 specific library).
  void* dl_handle = dlopen([[bundle_ executablePath] fileSystemRepresentation],
                           RTLD_LAZY | RTLD_LOCAL);
  if (dl_handle) {
    NSString* key = *reinterpret_cast<NSString**>(dlsym(dl_handle,
                                                        "kCWScanKeyMerge"));
    if (key)
      merge_key_.reset([key copy]);
  }
  // "Leak" dl_handle rather than dlclose it, to ensure |merge_key_|
  // remains valid.
  if (!merge_key_) {
    // Fall back to a known-working value should the lookup fail (if
    // this value is itself wrong it's not the end of the world, we might just
    // get very slightly lower quality location fixes due to SSID merges).
    DLOG(WARNING) << "Could not dynamically load the CoreWLAN merge key";
    merge_key_.reset([@"SCAN_MERGE" retain]);
  }

  return true;
}

bool CoreWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  base::mac::ScopedNSAutoreleasePool auto_pool;
  // Initialize the scan parameters with scan key merging disabled, so we get
  // every AP listed in the scan without any SSID de-duping logic.
  NSDictionary* params =
      [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO]
                                  forKey:merge_key_.get()];

  Class cw_interface_class = [bundle_ classNamed:@"CWInterface"];
  NSArray* supported_interfaces = [cw_interface_class supportedInterfaces];
  uint interface_error_count = 0;
  for (NSString* interface_name in supported_interfaces) {
    CWInterface* corewlan_interface =
        [cw_interface_class interfaceWithName:interface_name];
    if (!corewlan_interface) {
      DLOG(WARNING) << interface_name << ": initWithName failed";
      ++interface_error_count;
      continue;
    }

    const base::TimeTicks start_time = base::TimeTicks::Now();

    NSError* err = nil;
    NSArray* scan = [corewlan_interface scanForNetworksWithParameters:params
                                                                error:&err];
    const int error_code = [err code];
    const int count = [scan count];
    // We could get an error code but count != 0 if the scan was interrupted,
    // for example. For our purposes this is not fatal, so process as normal.
    if (error_code && count == 0) {
      DLOG(WARNING) << interface_name << ": CoreWLAN scan failed with error "
                    << error_code;
      ++interface_error_count;
      continue;
    }

    const base::TimeDelta duration = base::TimeTicks::Now() - start_time;

    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.Wifi.ScanLatency",
        duration,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(1),
        100);

    DVLOG(1) << interface_name << ": found " << count << " wifi APs";

    for (CWNetwork* network in scan) {
      DCHECK(network);
      AccessPointData access_point_data;
      NSData* mac = [network bssidData];
      DCHECK([mac length] == 6);
      if (![mac bytes])
        continue;  // crbug.com/545501
      access_point_data.mac_address =
          MacAddressAsString16(static_cast<const uint8_t*>([mac bytes]));
      access_point_data.radio_signal_strength = [network rssiValue];
      access_point_data.channel = [[network channel] intValue];
      access_point_data.signal_to_noise =
          access_point_data.radio_signal_strength - [[network noise] intValue];
      access_point_data.ssid = base::SysNSStringToUTF16([network ssid]);
      data->insert(access_point_data);
    }
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.Wifi.InterfaceCount",
      [supported_interfaces count] - interface_error_count,
      1,
      5,
      6);

  // Return true even if some interfaces failed to scan, so long as at least
  // one interface did not fail.
  return interface_error_count == 0 ||
         [supported_interfaces count] > interface_error_count;
}

WifiDataProviderCommon::WlanApiInterface* NewCoreWlanApi() {
  std::unique_ptr<CoreWlanApi> self(new CoreWlanApi);
  if (self->Init())
    return self.release();

  return NULL;
}

}  // namespace content
