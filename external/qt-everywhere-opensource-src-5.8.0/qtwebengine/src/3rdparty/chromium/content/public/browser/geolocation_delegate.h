// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GEOLOCATION_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_GEOLOCATION_DELEGATE_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {
class AccessTokenStore;
class LocationProvider;

// An embedder of Geolocation may override these class' methods to provide
// specific functionality.
class CONTENT_EXPORT GeolocationDelegate {
 public:
  // Returns true if the location API should use network-based location
  // approximation in addition to the system provider, if any.
  virtual bool UseNetworkLocationProviders();
   // Creates a new AccessTokenStore for geolocation. May return nullptr.
  // TODO(mcasas): consider changing it return type to std::unique_ptr<> to
  // clarify ownership, https://crbug.com/623114.
  virtual AccessTokenStore* CreateAccessTokenStore();
   // Allows an embedder to return its own LocationProvider implementation.
  // Return nullptr to use the default one for the platform to be created.
  // Caller takes ownership of the returned LocationProvider. FYI: Used by an
  // external project; please don't remove. Contact Viatcheslav Ostapenko at
  // sl.ostapenko@samsung.com for more information.
  // TODO(mcasas): return std::unique_ptr<> instead, https://crbug.com/623132.
  virtual LocationProvider* OverrideSystemLocationProvider();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GEOLOCATION_DELEGATE_H_
