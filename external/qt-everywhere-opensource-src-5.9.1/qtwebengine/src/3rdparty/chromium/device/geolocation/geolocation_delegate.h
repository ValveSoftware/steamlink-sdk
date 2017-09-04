// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_GEOLOCATION_DELEGATE_H_
#define DEVICE_GEOLOCATION_GEOLOCATION_DELEGATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "device/geolocation/geolocation_export.h"

namespace device {
class AccessTokenStore;
class LocationProvider;

// An embedder of Geolocation may override these class' methods to provide
// specific functionality.
class DEVICE_GEOLOCATION_EXPORT GeolocationDelegate {
 public:
  virtual ~GeolocationDelegate() {}

  // Returns true if the location API should use network-based location
  // approximation in addition to the system provider, if any.
  virtual bool UseNetworkLocationProviders();

  // Creates a new AccessTokenStore for geolocation. May return nullptr.
  virtual scoped_refptr<AccessTokenStore> CreateAccessTokenStore();

  // Allows an embedder to return its own LocationProvider implementation.
  // Return nullptr to use the default one for the platform to be created.
  // FYI: Used by an external project; please don't remove. Contact Viatcheslav
  // Ostapenko at sl.ostapenko@samsung.com for more information.
  virtual std::unique_ptr<LocationProvider> OverrideSystemLocationProvider();
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_GEOLOCATION_DELEGATE_H_
