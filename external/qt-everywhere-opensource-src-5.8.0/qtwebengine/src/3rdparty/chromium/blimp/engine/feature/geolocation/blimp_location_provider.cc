// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/blimp_location_provider.h"

#include "content/public/common/geoposition.h"

namespace blimp {
namespace engine {

BlimpLocationProvider::BlimpLocationProvider() {}

BlimpLocationProvider::~BlimpLocationProvider() {
  StopProvider();
}

bool BlimpLocationProvider::StartProvider(bool high_accuracy) {
  NOTIMPLEMENTED();
  return true;
}

void BlimpLocationProvider::StopProvider() {
}

void BlimpLocationProvider::GetPosition(content::Geoposition* position) {
  DCHECK(position);

  *position = position_;
}

void BlimpLocationProvider::RequestRefresh() {
  NOTIMPLEMENTED();
}

void BlimpLocationProvider::OnPermissionGranted() {
  RequestRefresh();
  NOTIMPLEMENTED();
}

void BlimpLocationProvider::NotifyCallback(
    const content::Geoposition& position) {
  DCHECK(!callback_.is_null());

  callback_.Run(this, position);
}

void BlimpLocationProvider::OnLocationResponse(
    const content::Geoposition& position) {
  NotifyCallback(position);
  NOTIMPLEMENTED();
}

void BlimpLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(!callback.is_null());

  callback_ = callback;
}

}  // namespace engine
}  // namespace blimp
