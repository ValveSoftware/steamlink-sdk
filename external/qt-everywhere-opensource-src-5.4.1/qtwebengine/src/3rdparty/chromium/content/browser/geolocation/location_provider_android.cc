// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_provider_android.h"

#include "base/time/time.h"
#include "content/browser/geolocation/location_api_adapter_android.h"
#include "content/public/common/geoposition.h"

namespace content {

// LocationProviderAndroid
LocationProviderAndroid::LocationProviderAndroid() {
}

LocationProviderAndroid::~LocationProviderAndroid() {
  StopProvider();
}

void LocationProviderAndroid::NotifyNewGeoposition(
    const Geoposition& position) {
  last_position_ = position;
  NotifyCallback(last_position_);
}

bool LocationProviderAndroid::StartProvider(bool high_accuracy) {
  return AndroidLocationApiAdapter::GetInstance()->Start(this, high_accuracy);
}

void LocationProviderAndroid::StopProvider() {
  AndroidLocationApiAdapter::GetInstance()->Stop();
}

void LocationProviderAndroid::GetPosition(Geoposition* position) {
  *position = last_position_;
}

void LocationProviderAndroid::RequestRefresh() {
  // Nothing to do here, android framework will call us back on new position.
}

void LocationProviderAndroid::OnPermissionGranted() {
  // Nothing to do here.
}

LocationProvider* NewSystemLocationProvider() {
  return new LocationProviderAndroid;
}

}  // namespace content
