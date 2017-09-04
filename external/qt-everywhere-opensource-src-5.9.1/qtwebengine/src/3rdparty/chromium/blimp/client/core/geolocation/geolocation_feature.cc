// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/geolocation/geolocation_feature.h"

#include <memory>
#include <string>
#include <utility>

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

GeolocationFeature::GeolocationFeature(
    std::unique_ptr<device::LocationProvider> location_provider)
    : location_provider_(std::move(location_provider)) {
  location_provider_->SetUpdateCallback(base::Bind(
      &GeolocationFeature::OnLocationUpdate, base::Unretained(this)));
}

GeolocationFeature::~GeolocationFeature() {}

void GeolocationFeature::set_outgoing_message_processor(
    std::unique_ptr<BlimpMessageProcessor> processor) {
  outgoing_message_processor_ = std::move(processor);
  am_sending_message_ = false;
}

void GeolocationFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(BlimpMessage::kGeolocation, message->feature_case());

  int result = net::OK;
  const GeolocationMessage& geolocation_message = message->geolocation();
  switch (geolocation_message.type_case()) {
    case GeolocationMessage::kSetInterestLevel:
      SetInterestLevel(geolocation_message.set_interest_level().level());
      break;
    case GeolocationMessage::kRequestRefresh:
      location_provider_->OnPermissionGranted();
      break;
    case GeolocationMessage::kCoordinates:
    case GeolocationMessage::kError:
    case GeolocationMessage::TYPE_NOT_SET:
      result = net::ERR_UNEXPECTED;
      break;
  }

  if (!callback.is_null()) {
    callback.Run(result);
  }
}

void GeolocationFeature::OnLocationUpdate(
    const device::LocationProvider* location_provider,
    const device::Geoposition& position) {
  DCHECK_EQ(location_provider_.get(), location_provider);

  if (!am_sending_message_) {
    switch (position.error_code) {
      case device::Geoposition::ERROR_CODE_NONE:
        SendGeolocationPositionMessage(position);
        break;
      case device::Geoposition::ErrorCode::ERROR_CODE_PERMISSION_DENIED:
        SendGeolocationErrorMessage(GeolocationErrorMessage::PERMISSION_DENIED,
                                    position.error_message);
        break;
      case device::Geoposition::ErrorCode::ERROR_CODE_POSITION_UNAVAILABLE:
        SendGeolocationErrorMessage(
            GeolocationErrorMessage::POSITION_UNAVAILABLE,
            position.error_message);
        break;
      case device::Geoposition::ErrorCode::ERROR_CODE_TIMEOUT:
        SendGeolocationErrorMessage(GeolocationErrorMessage::TIMEOUT,
                                    position.error_message);
        break;
    }
  } else {
    need_to_send_message_ = true;
  }
}

void GeolocationFeature::SetInterestLevel(
    GeolocationSetInterestLevelMessage::Level level) {
  switch (level) {
    case GeolocationSetInterestLevelMessage::HIGH_ACCURACY:
      location_provider_->StartProvider(true);
      break;
    case GeolocationSetInterestLevelMessage::LOW_ACCURACY:
      location_provider_->StartProvider(false);
      break;
    case GeolocationSetInterestLevelMessage::NO_INTEREST:
      location_provider_->StopProvider();
      break;
  }
}

void GeolocationFeature::SendGeolocationPositionMessage(
    const device::Geoposition& position) {
  GeolocationMessage* geolocation_message = nullptr;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&geolocation_message);

  GeolocationCoordinatesMessage* coordinates =
      geolocation_message->mutable_coordinates();
  coordinates->set_latitude(position.latitude);
  coordinates->set_longitude(position.longitude);
  coordinates->set_altitude(position.altitude);
  coordinates->set_accuracy(position.accuracy);
  coordinates->set_altitude_accuracy(position.altitude_accuracy);
  coordinates->set_heading(position.heading);
  coordinates->set_speed(position.speed);

  am_sending_message_ = true;
  outgoing_message_processor_->ProcessMessage(
      std::move(blimp_message),
      base::Bind(&GeolocationFeature::OnSendComplete, base::Unretained(this)));
}

void GeolocationFeature::SendGeolocationErrorMessage(
    GeolocationErrorMessage::ErrorCode error_code,
    const std::string& error_message) {
  GeolocationMessage* geolocation_message = nullptr;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&geolocation_message);

  GeolocationErrorMessage* error = geolocation_message->mutable_error();
  error->set_error_code(error_code);
  error->set_error_message(error_message);

  am_sending_message_ = true;
  outgoing_message_processor_->ProcessMessage(
      std::move(blimp_message),
      base::Bind(&GeolocationFeature::OnSendComplete, base::Unretained(this)));
}

void GeolocationFeature::OnSendComplete(int result) {
  am_sending_message_ = false;
  if (need_to_send_message_) {
    device::Geoposition new_position = location_provider_->GetPosition();
    if (new_position.Validate()) {
      OnLocationUpdate(location_provider_.get(), new_position);
    }
    need_to_send_message_ = false;
  }
}

}  // namespace client
}  // namespace blimp
