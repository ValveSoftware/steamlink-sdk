// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "device/geolocation/geolocation_provider_impl.h"
#include "device/geolocation/public/interfaces/geolocation.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

#ifndef DEVICE_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
#define DEVICE_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_

namespace device {

class GeolocationProvider;
class GeolocationServiceContext;

// Implements the GeolocationService Mojo interface.
class GeolocationServiceImpl : public mojom::GeolocationService {
 public:
  // |context| must outlive this object. |update_callback| will be called when
  // location updates are sent, allowing the client to know when the service
  // is being used.
  GeolocationServiceImpl(
      mojo::InterfaceRequest<mojom::GeolocationService> request,
      GeolocationServiceContext* context,
      const base::Closure& update_callback);
  ~GeolocationServiceImpl() override;

  // Starts listening for updates.
  void StartListeningForUpdates();

  // Pauses and resumes sending updates to the client of this instance.
  void PauseUpdates();
  void ResumeUpdates();

  // Enables and disables geolocation override.
  void SetOverride(const Geoposition& position);
  void ClearOverride();

 private:
  // mojom::GeolocationService:
  void SetHighAccuracy(bool high_accuracy) override;
  void QueryNextPosition(const QueryNextPositionCallback& callback) override;

  void OnConnectionError();

  void OnLocationUpdate(const Geoposition& position);
  void ReportCurrentPosition();

  // The binding between this object and the other end of the pipe.
  mojo::Binding<mojom::GeolocationService> binding_;

  // Owns this object.
  GeolocationServiceContext* context_;
  std::unique_ptr<GeolocationProvider::Subscription> geolocation_subscription_;

  // Callback that allows the instantiator of this class to be notified on
  // position updates.
  base::Closure update_callback_;

  // The callback passed to QueryNextPosition.
  QueryNextPositionCallback position_callback_;

  // Valid iff SetOverride() has been called and ClearOverride() has not
  // subsequently been called.
  Geoposition position_override_;

  mojom::Geoposition current_position_;

  // Whether this instance is currently observing location updates with high
  // accuracy.
  bool high_accuracy_;

  bool has_position_to_report_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceImpl);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
