// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/engine_geolocation_feature.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/geolocation.pb.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/location_provider.h"
#include "device/geolocation/geoposition.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace engine {
namespace {

class BlimpGeolocationDelegate : public device::GeolocationDelegate {
 public:
  explicit BlimpGeolocationDelegate(
      base::WeakPtr<BlimpLocationProvider::Delegate> feature_delegate) {
    feature_delegate_ = feature_delegate;
    feature_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  bool UseNetworkLocationProviders() final { return false; }

  std::unique_ptr<device::LocationProvider> OverrideSystemLocationProvider()
      final {
    return base::MakeUnique<BlimpLocationProvider>(feature_delegate_,
                                                   feature_task_runner_);
  }

 private:
  base::WeakPtr<BlimpLocationProvider::Delegate> feature_delegate_;
  scoped_refptr<base::SingleThreadTaskRunner> feature_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BlimpGeolocationDelegate);
};

device::Geoposition::ErrorCode ConvertErrorCode(
    const GeolocationErrorMessage::ErrorCode& error_code) {
  switch (error_code) {
    case GeolocationErrorMessage::PERMISSION_DENIED:
      return device::Geoposition::ErrorCode::ERROR_CODE_PERMISSION_DENIED;
    case GeolocationErrorMessage::POSITION_UNAVAILABLE:
      return device::Geoposition::ErrorCode::ERROR_CODE_POSITION_UNAVAILABLE;
    case GeolocationErrorMessage::TIMEOUT:
      return device::Geoposition::ErrorCode::ERROR_CODE_TIMEOUT;
  }
}

device::Geoposition ConvertLocationMessage(
    const GeolocationCoordinatesMessage& coordinates) {
  device::Geoposition output;
  output.latitude = coordinates.latitude();
  output.longitude = coordinates.longitude();
  output.altitude = coordinates.altitude();
  output.accuracy = coordinates.accuracy();
  output.altitude_accuracy = coordinates.altitude_accuracy();
  output.heading = coordinates.heading();
  output.speed = coordinates.speed();
  output.timestamp = base::Time::Now();
  output.error_code = device::Geoposition::ErrorCode::ERROR_CODE_NONE;
  return output;
}

}  // namespace

EngineGeolocationFeature::EngineGeolocationFeature() : weak_factory_(this) {}

EngineGeolocationFeature::~EngineGeolocationFeature() {}

void EngineGeolocationFeature::set_outgoing_message_processor(
    std::unique_ptr<BlimpMessageProcessor> message_processor) {
  DCHECK(message_processor);
  outgoing_message_processor_ = std::move(message_processor);
}

device::GeolocationDelegate*
EngineGeolocationFeature::CreateGeolocationDelegate() {
  return new BlimpGeolocationDelegate(weak_factory_.GetWeakPtr());
}

void EngineGeolocationFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(BlimpMessage::kGeolocation, message->feature_case());

  int result = net::OK;
  const GeolocationMessage& geolocation_message = message->geolocation();
  switch (geolocation_message.type_case()) {
    case GeolocationMessage::kCoordinates: {
      const GeolocationCoordinatesMessage& location =
          geolocation_message.coordinates();
      device::Geoposition output = ConvertLocationMessage(location);
      NotifyCallback(output);
      break;
    }
    case GeolocationMessage::kError: {
      const GeolocationErrorMessage& error_message =
          geolocation_message.error();
      device::Geoposition output;
      output.error_message = error_message.error_message();
      output.error_code = ConvertErrorCode(error_message.error_code());
      NotifyCallback(output);
      break;
    }
    case GeolocationMessage::kSetInterestLevel:
    case GeolocationMessage::kRequestRefresh:
    case GeolocationMessage::TYPE_NOT_SET:
      result = net::ERR_UNEXPECTED;
  }
  if (!callback.is_null()) {
    callback.Run(result);
  }
}

void EngineGeolocationFeature::NotifyCallback(
    const device::Geoposition& position) {
  geoposition_received_callback_.Run(position);
}

void EngineGeolocationFeature::RequestAccuracy(
    GeolocationSetInterestLevelMessage::Level level) {
  GeolocationMessage* geolocation_message = nullptr;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&geolocation_message);

  GeolocationSetInterestLevelMessage* geolocation_interest =
      geolocation_message->mutable_set_interest_level();
  geolocation_interest->set_level(level);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void EngineGeolocationFeature::OnPermissionGranted() {
  GeolocationMessage* geolocation_message = nullptr;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&geolocation_message);

  geolocation_message->mutable_request_refresh();

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void EngineGeolocationFeature::SetUpdateCallback(
    const GeopositionReceivedCallback& callback) {
  geoposition_received_callback_ = callback;
}

}  // namespace engine
}  // namespace blimp
