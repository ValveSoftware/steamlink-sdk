// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
#define DEVICE_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_

#include "base/compiler_specific.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"

namespace device {
class AndroidLocationApiAdapter;
struct Geoposition;

// Location provider for Android using the platform provider over JNI.
class LocationProviderAndroid : public LocationProvider {
 public:
  LocationProviderAndroid();
  ~LocationProviderAndroid() override;

  // Called by the AndroidLocationApiAdapter.
  void NotifyNewGeoposition(const Geoposition& position);

  // LocationProvider.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 private:
  Geoposition last_position_;
  LocationProviderUpdateCallback callback_;
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
