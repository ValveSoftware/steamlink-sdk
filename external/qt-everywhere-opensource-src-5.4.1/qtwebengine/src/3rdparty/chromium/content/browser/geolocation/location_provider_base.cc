// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_provider_base.h"

namespace content {

LocationProviderBase::LocationProviderBase() {
}

LocationProviderBase::~LocationProviderBase() {
}

void LocationProviderBase::NotifyCallback(const Geoposition& position) {
  if (!callback_.is_null())
    callback_.Run(this, position);
}

void LocationProviderBase::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;
}

void LocationProviderBase::RequestRefresh() {
}

}  // namespace content
