// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares the Geoposition structure, used to represent a position
// fix. It was originally derived from:
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation.h

#ifndef DEVICE_GEOLOCATION_GEOPOSITION_H_
#define DEVICE_GEOLOCATION_GEOPOSITION_H_

#include <string>

#include "base/time/time.h"
#include "device/geolocation/geolocation_export.h"

namespace device {

struct DEVICE_GEOLOCATION_EXPORT Geoposition {
 public:
  // These values follow the W3C geolocation specification and can be returned
  // to JavaScript without the need for a conversion.
  enum ErrorCode {
    ERROR_CODE_NONE = 0,  // Chrome addition.
    ERROR_CODE_PERMISSION_DENIED = 1,
    ERROR_CODE_POSITION_UNAVAILABLE = 2,
    ERROR_CODE_TIMEOUT = 3,
    ERROR_CODE_LAST = ERROR_CODE_TIMEOUT
  };

  // All fields are initialized to sentinel values marking them as invalid. The
  // error code is set to ERROR_CODE_NONE.
  Geoposition();

  Geoposition(const Geoposition& other);

  // A valid fix has a valid latitude, longitude, accuracy and timestamp.
  bool Validate() const;

  // These properties correspond to those of the JavaScript Position object
  // although their types may differ.
  // Latitude in decimal degrees north (WGS84 coordinate frame).
  double latitude;
  // Longitude in decimal degrees west (WGS84 coordinate frame).
  double longitude;
  // Altitude in meters (above WGS84 datum).
  double altitude;
  // Accuracy of horizontal position in meters.
  double accuracy;
  // Accuracy of altitude in meters.
  double altitude_accuracy;
  // Heading in decimal degrees clockwise from true north.
  double heading;
  // Horizontal component of device velocity in meters per second.
  double speed;
  // Time of position measurement in milisecons since Epoch in UTC time. This is
  // taken from the host computer's system clock (i.e. from Time::Now(), not the
  // source device's clock).
  base::Time timestamp;

  // Error code, see enum above.
  ErrorCode error_code;
  // Human-readable error message.
  std::string error_message;
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_GEOPOSITION_H_
