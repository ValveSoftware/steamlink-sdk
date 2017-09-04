// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
#define DEVICE_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_

#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockLocationProvider : public device::LocationProvider {
 public:
  MockLocationProvider();
  ~MockLocationProvider() override;

  MOCK_METHOD1(SetUpdateCallback,
               void(const LocationProviderUpdateCallback& callback));
  MOCK_METHOD1(StartProvider, bool(bool high_accuracy));
  MOCK_METHOD0(StopProvider, void());
  MOCK_METHOD0(GetPosition, const Geoposition&());
  MOCK_METHOD0(OnPermissionGranted, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLocationProvider);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
