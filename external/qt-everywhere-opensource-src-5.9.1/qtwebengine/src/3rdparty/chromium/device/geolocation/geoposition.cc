// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geoposition.h"

namespace {
// Sentinel values to mark invalid data. (WebKit carries companion is_valid
// bools for this purpose; we may eventually follow that approach, but
// sentinels worked OK in the Gears code this is based on.)
const double kBadLatitudeLongitude = 200;
// Lowest point on land is at approximately -400 meters.
const int kBadAltitude = -10000;
const int kBadAccuracy = -1;  // Accuracy must be non-negative.
const int kBadHeading = -1;   // Heading must be non-negative.
const int kBadSpeed = -1;
}

namespace device {

Geoposition::Geoposition()
    : latitude(kBadLatitudeLongitude),
      longitude(kBadLatitudeLongitude),
      altitude(kBadAltitude),
      accuracy(kBadAccuracy),
      altitude_accuracy(kBadAccuracy),
      heading(kBadHeading),
      speed(kBadSpeed),
      error_code(ERROR_CODE_NONE) {}

Geoposition::Geoposition(const Geoposition& other) = default;

bool Geoposition::Validate() const {
  return latitude >= -90. && latitude <= 90. && longitude >= -180. &&
         longitude <= 180. && accuracy >= 0. && !timestamp.is_null();
}

}  // namespace device
