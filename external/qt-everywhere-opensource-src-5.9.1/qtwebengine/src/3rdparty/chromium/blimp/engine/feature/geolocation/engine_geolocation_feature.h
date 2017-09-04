// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_GEOLOCATION_ENGINE_GEOLOCATION_FEATURE_H_
#define BLIMP_ENGINE_FEATURE_GEOLOCATION_ENGINE_GEOLOCATION_FEATURE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "blimp/engine/feature/geolocation/blimp_location_provider.h"
#include "blimp/net/blimp_message_processor.h"

namespace device {
class GeolocationDelegate;
struct Geoposition;
}

namespace blimp {
namespace engine {

// Handles all incoming and outgoing protobuf messages types tied to
// geolocation.
class EngineGeolocationFeature : public BlimpMessageProcessor,
                                 public BlimpLocationProvider::Delegate {
 public:
  EngineGeolocationFeature();
  ~EngineGeolocationFeature() override;

  void set_outgoing_message_processor(
      std::unique_ptr<BlimpMessageProcessor> message_processor);

  device::GeolocationDelegate* CreateGeolocationDelegate();

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  void NotifyCallback(const device::Geoposition& position);

  // BlimpLocationProvider::Delegate implementation.
  void RequestAccuracy(
      GeolocationSetInterestLevelMessage::Level level) override;
  void OnPermissionGranted() override;
  void SetUpdateCallback(const GeopositionReceivedCallback& callback) override;

  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;
  GeopositionReceivedCallback geoposition_received_callback_;
  base::WeakPtrFactory<EngineGeolocationFeature> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EngineGeolocationFeature);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_GEOLOCATION_ENGINE_GEOLOCATION_FEATURE_H_
