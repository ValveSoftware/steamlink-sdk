// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_

#include "base/compiler_specific.h"
#include "content/browser/geolocation/location_provider_base.h"
#include "content/public/common/geoposition.h"

namespace content {
class AndroidLocationApiAdapter;
struct Geoposition;

// Location provider for Android using the platform provider over JNI.
class LocationProviderAndroid : public LocationProviderBase {
 public:
  LocationProviderAndroid();
  virtual ~LocationProviderAndroid();

  // Called by the AndroidLocationApiAdapter.
  void NotifyNewGeoposition(const Geoposition& position);

  // LocationProvider.
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition* position) OVERRIDE;
  virtual void RequestRefresh() OVERRIDE;
  virtual void OnPermissionGranted() OVERRIDE;

 private:
  Geoposition last_position_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
