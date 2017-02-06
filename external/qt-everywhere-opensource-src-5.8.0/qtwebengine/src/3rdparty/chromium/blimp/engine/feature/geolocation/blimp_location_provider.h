// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_
#define BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_

#include "content/public/browser/location_provider.h"
#include "content/public/common/geoposition.h"

namespace blimp {
namespace engine {

// Location provider for Blimp using the device's provider over the network.
class BlimpLocationProvider : public content::LocationProvider {
 public:
  BlimpLocationProvider();
  ~BlimpLocationProvider() override;

  // content::LocationProvider implementation.
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  void GetPosition(content::Geoposition* position) override;
  void RequestRefresh() override;
  void OnPermissionGranted() override;

 private:
  void NotifyCallback(const content::Geoposition& position);
  void OnLocationResponse(const content::Geoposition& position);
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;

  LocationProviderUpdateCallback callback_;

  content::Geoposition position_;

  DISALLOW_COPY_AND_ASSIGN(BlimpLocationProvider);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_
