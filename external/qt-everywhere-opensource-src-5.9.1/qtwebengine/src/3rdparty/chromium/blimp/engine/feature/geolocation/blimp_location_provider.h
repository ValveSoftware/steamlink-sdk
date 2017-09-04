// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_
#define BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "blimp/common/proto/geolocation.pb.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"

namespace blimp {
namespace engine {

// Location provider for Blimp using the device's provider over the network.
class BlimpLocationProvider : public device::LocationProvider {
 public:
  // A delegate that implements a subset of LocationProvider's functions.
  // All methods will be executed on |delegate_task_runner_|.
  class Delegate {
   public:
    using GeopositionReceivedCallback =
        base::Callback<void(const device::Geoposition&)>;
    virtual ~Delegate() {}

    virtual void RequestAccuracy(
        GeolocationSetInterestLevelMessage::Level level) = 0;
    virtual void OnPermissionGranted() = 0;
    virtual void SetUpdateCallback(
        const GeopositionReceivedCallback& callback) = 0;
  };

  BlimpLocationProvider(
      base::WeakPtr<Delegate> delegate,
      scoped_refptr<base::SequencedTaskRunner> delegate_task_runner);
  ~BlimpLocationProvider() override;

  // device::LocationProvider implementation.
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const device::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;

 private:
  void OnLocationUpdate(const device::Geoposition& geoposition);

  // This delegate handles a subset of the LocationProvider functionality.
  base::WeakPtr<Delegate> delegate_;

  scoped_refptr<base::SequencedTaskRunner> delegate_task_runner_;
  device::Geoposition cached_position_;

  // True if a successful StartProvider call has occured.
  bool is_started_;

  LocationProviderUpdateCallback location_update_callback_;

  base::WeakPtrFactory<BlimpLocationProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpLocationProvider);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_
