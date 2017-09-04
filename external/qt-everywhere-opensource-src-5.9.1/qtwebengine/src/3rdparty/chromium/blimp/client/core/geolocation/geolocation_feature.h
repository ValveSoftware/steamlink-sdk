// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_GEOLOCATION_GEOLOCATION_FEATURE_H_
#define BLIMP_CLIENT_CORE_GEOLOCATION_GEOLOCATION_FEATURE_H_

#include <memory>
#include <string>

#include "blimp/common/proto/geolocation.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"

namespace blimp {
namespace client {

// Client-side feature handling geolocation messages.
class GeolocationFeature : public BlimpMessageProcessor {
 public:
  explicit GeolocationFeature(
      std::unique_ptr<device::LocationProvider> location_provider);
  ~GeolocationFeature() override;

  // Sets the BlimpMessageProcessor that will be used to send
  // BlimpMessage::GEOLOCATION messages to the engine.
  void set_outgoing_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // Sends engine an update of the client's geoposition.
  void OnLocationUpdate(const device::LocationProvider* location_provider,
                        const device::Geoposition& position);

  // Handles a request from the Engine to change the accuracy level of the
  // Geolocation information reported to it.
  void SetInterestLevel(GeolocationSetInterestLevelMessage::Level level);

  // Sends a GeolocationPositionMessage that reflects the given
  // geoposition.
  void SendGeolocationPositionMessage(const device::Geoposition& position);

  // Sends a GeolocationErrorMessage.
  void SendGeolocationErrorMessage(
      GeolocationErrorMessage::ErrorCode error_code,
      const std::string& error_message);

  // Called when a message send completes.
  void OnSendComplete(int result);

  // Used to obtain the client's location.
  std::unique_ptr<device::LocationProvider> location_provider_;

  // Used to send BlimpMessage::GEOLOCATION message to the engine.
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;

  // True if a message is in the process of being sent.
  bool am_sending_message_ = false;

  // True if location has been updated but a message cannot be sent at the
  // moment.
  bool need_to_send_message_ = false;

  DISALLOW_COPY_AND_ASSIGN(GeolocationFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_GEOLOCATION_GEOLOCATION_FEATURE_H_
