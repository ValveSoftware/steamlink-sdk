// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_service_context.h"

#include <utility>

#include "device/geolocation/geolocation_service_impl.h"

namespace device {

GeolocationServiceContext::GeolocationServiceContext() {}

GeolocationServiceContext::~GeolocationServiceContext() {}

void GeolocationServiceContext::CreateService(
    const base::Closure& update_callback,
    mojo::InterfaceRequest<mojom::GeolocationService> request) {
  GeolocationServiceImpl* service =
      new GeolocationServiceImpl(std::move(request), this, update_callback);
  services_.push_back(service);
  if (geoposition_override_)
    service->SetOverride(*geoposition_override_.get());
  else
    service->StartListeningForUpdates();
}

void GeolocationServiceContext::ServiceHadConnectionError(
    GeolocationServiceImpl* service) {
  auto it = std::find(services_.begin(), services_.end(), service);
  DCHECK(it != services_.end());
  services_.erase(it);
}

void GeolocationServiceContext::SetOverride(
    std::unique_ptr<Geoposition> geoposition) {
  geoposition_override_.swap(geoposition);
  for (auto* service : services_) {
    service->SetOverride(*geoposition_override_.get());
  }
}

void GeolocationServiceContext::ClearOverride() {
  for (auto* service : services_) {
    service->ClearOverride();
  }
}

}  // namespace device
