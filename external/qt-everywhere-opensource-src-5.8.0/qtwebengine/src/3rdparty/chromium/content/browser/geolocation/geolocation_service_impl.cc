// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "content/browser/geolocation/geolocation_service_context.h"

namespace content {

namespace {

// Geoposition error codes for reporting in UMA.
enum GeopositionErrorCode {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.

  // There was no error.
  GEOPOSITION_ERROR_CODE_NONE = 0,

  // User denied use of geolocation.
  GEOPOSITION_ERROR_CODE_PERMISSION_DENIED = 1,

  // Geoposition could not be determined.
  GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE = 2,

  // Timeout.
  GEOPOSITION_ERROR_CODE_TIMEOUT = 3,

  // NOTE: Add entries only immediately above this line.
  GEOPOSITION_ERROR_CODE_COUNT = 4
};

void RecordGeopositionErrorCode(Geoposition::ErrorCode error_code) {
  GeopositionErrorCode code = GEOPOSITION_ERROR_CODE_NONE;
  switch (error_code) {
    case Geoposition::ERROR_CODE_NONE:
      code = GEOPOSITION_ERROR_CODE_NONE;
      break;
    case Geoposition::ERROR_CODE_PERMISSION_DENIED:
      code = GEOPOSITION_ERROR_CODE_PERMISSION_DENIED;
      break;
    case Geoposition::ERROR_CODE_POSITION_UNAVAILABLE:
      code = GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE;
      break;
    case Geoposition::ERROR_CODE_TIMEOUT:
      code = GEOPOSITION_ERROR_CODE_TIMEOUT;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Geolocation.LocationUpdate.ErrorCode",
                            code,
                            GEOPOSITION_ERROR_CODE_COUNT);
}

}  // namespace

GeolocationServiceImpl::GeolocationServiceImpl(
    mojo::InterfaceRequest<GeolocationService> request,
    GeolocationServiceContext* context,
    const base::Closure& update_callback)
    : binding_(this, std::move(request)),
      context_(context),
      update_callback_(update_callback),
      high_accuracy_(false),
      has_position_to_report_(false) {
  DCHECK(context_);
  binding_.set_connection_error_handler(
      base::Bind(&GeolocationServiceImpl::OnConnectionError,
                 base::Unretained(this)));
}

GeolocationServiceImpl::~GeolocationServiceImpl() {
  // Make sure to respond to any pending callback even without a valid position.
  if (!position_callback_.is_null()) {
    if (!current_position_.valid) {
      current_position_.error_code = blink::mojom::Geoposition::ErrorCode(
          GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE);
      current_position_.error_message = mojo::String("");
    }
    ReportCurrentPosition();
  }
}

void GeolocationServiceImpl::PauseUpdates() {
  geolocation_subscription_.reset();
}

void GeolocationServiceImpl::ResumeUpdates() {
  if (position_override_.Validate()) {
    OnLocationUpdate(position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationServiceImpl::StartListeningForUpdates() {
  geolocation_subscription_ =
      GeolocationProvider::GetInstance()->AddLocationUpdateCallback(
          base::Bind(&GeolocationServiceImpl::OnLocationUpdate,
                     base::Unretained(this)),
          high_accuracy_);
}

void GeolocationServiceImpl::SetHighAccuracy(bool high_accuracy) {
  UMA_HISTOGRAM_BOOLEAN(
      "Geolocation.GeolocationDispatcherHostImpl.EnableHighAccuracy",
      high_accuracy);
  high_accuracy_ = high_accuracy;

  if (position_override_.Validate()) {
    OnLocationUpdate(position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationServiceImpl::QueryNextPosition(
    const QueryNextPositionCallback& callback) {
  if (!position_callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextPosition!";
    OnConnectionError();  // Simulate a connection error.
    return;
  }

  position_callback_ = callback;

  if (has_position_to_report_)
    ReportCurrentPosition();
}

void GeolocationServiceImpl::SetOverride(const Geoposition& position) {
  position_override_ = position;
  if (!position_override_.Validate()) {
    ResumeUpdates();
  }

  geolocation_subscription_.reset();

  OnLocationUpdate(position_override_);
}

void GeolocationServiceImpl::ClearOverride() {
  position_override_ = Geoposition();
  StartListeningForUpdates();
}

void GeolocationServiceImpl::OnConnectionError() {
  context_->ServiceHadConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void GeolocationServiceImpl::OnLocationUpdate(const Geoposition& position) {
  RecordGeopositionErrorCode(position.error_code);
  DCHECK(context_);

  if (context_->paused())
    return;

  update_callback_.Run();

  current_position_.valid = position.Validate();
  current_position_.latitude = position.latitude;
  current_position_.longitude = position.longitude;
  current_position_.altitude = position.altitude;
  current_position_.accuracy = position.accuracy;
  current_position_.altitude_accuracy = position.altitude_accuracy;
  current_position_.heading = position.heading;
  current_position_.speed = position.speed;
  current_position_.timestamp = position.timestamp.ToDoubleT();
  current_position_.error_code =
      blink::mojom::Geoposition::ErrorCode(position.error_code);
  current_position_.error_message = position.error_message;

  has_position_to_report_ = true;

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void GeolocationServiceImpl::ReportCurrentPosition() {
  position_callback_.Run(current_position_.Clone());
  position_callback_.Reset();
  has_position_to_report_ = false;
}

}  // namespace content
