// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/mock_location_arbitrator.h"

#include "base/message_loop/message_loop.h"
#include "content/public/common/geoposition.h"

namespace content {

MockLocationArbitrator::MockLocationArbitrator()
    : permission_granted_(false),
      providers_started_(false) {
}

void MockLocationArbitrator::StartProviders(bool use_high_accuracy) {
  providers_started_ = true;;
}

void MockLocationArbitrator::StopProviders() {
  providers_started_ = false;
}

void MockLocationArbitrator::OnPermissionGranted() {
  permission_granted_ = true;
}

bool MockLocationArbitrator::HasPermissionBeenGranted() const {
  return permission_granted_;
}

}  // namespace content
