// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_IMPL_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/common/content_export.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/location_provider.h"
#include "content/public/common/geoposition.h"
#include "net/url_request/url_request_context_getter.h"

#include <set>

namespace net {
class URLRequestContextGetter;
}

namespace content {
class AccessTokenStore;
class LocationProvider;

// This class is responsible for handling updates from multiple underlying
// providers and resolving them to a single 'best' location fix at any given
// moment.
class CONTENT_EXPORT LocationArbitratorImpl : public LocationArbitrator {
 public:
  // Number of milliseconds newer a location provider has to be that it's worth
  // switching to this location provider on the basis of it being fresher
  // (regardles of relative accuracy). Public for tests.
  static const int64 kFixStaleTimeoutMilliseconds;

  typedef base::Callback<void(const Geoposition&)> LocationUpdateCallback;

  explicit LocationArbitratorImpl(const LocationUpdateCallback& callback);
  virtual ~LocationArbitratorImpl();

  static GURL DefaultNetworkProviderURL();

  // LocationArbitrator
  virtual void StartProviders(bool use_high_accuracy) OVERRIDE;
  virtual void StopProviders() OVERRIDE;
  virtual void OnPermissionGranted() OVERRIDE;
  virtual bool HasPermissionBeenGranted() const OVERRIDE;

 protected:
  AccessTokenStore* GetAccessTokenStore();

  // These functions are useful for injection of dependencies in derived
  // testing classes.
  virtual AccessTokenStore* NewAccessTokenStore();
  virtual LocationProvider* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      net::URLRequestContextGetter* context,
      const GURL& url,
      const base::string16& access_token);
  virtual LocationProvider* NewSystemLocationProvider();
  virtual base::Time GetTimeNow() const;

 private:
  // Takes ownership of |provider| on entry; it will either be added to
  // |providers_| or deleted on error (e.g. it fails to start).
  void RegisterProvider(LocationProvider* provider);
  void OnAccessTokenStoresLoaded(
      AccessTokenStore::AccessTokenSet access_token_store,
      net::URLRequestContextGetter* context_getter);
  void DoStartProviders();

  // The providers call this function when a new position is available.
  void LocationUpdateAvailable(const LocationProvider* provider,
                               const Geoposition& new_position);

  // Returns true if |new_position| is an improvement over |old_position|.
  // Set |from_same_provider| to true if both the positions came from the same
  // provider.
  bool IsNewPositionBetter(const Geoposition& old_position,
                           const Geoposition& new_position,
                           bool from_same_provider) const;

  scoped_refptr<AccessTokenStore> access_token_store_;
  LocationUpdateCallback callback_;
  LocationProvider::LocationProviderUpdateCallback provider_callback_;
  ScopedVector<LocationProvider> providers_;
  bool use_high_accuracy_;
  // The provider which supplied the current |position_|
  const LocationProvider* position_provider_;
  bool is_permission_granted_;
  // The current best estimate of our position.
  Geoposition position_;

  // Used to track if all providers had a chance to provide a location.
  std::set<const LocationProvider*> providers_polled_;

  // Tracks whether providers should be running.
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(LocationArbitratorImpl);
};

// Factory functions for the various types of location provider to abstract
// over the platform-dependent implementations.
LocationProvider* NewSystemLocationProvider();

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_IMPL_H_
