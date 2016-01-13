// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/common/geoposition.h"

template<typename Type> struct DefaultSingletonTraits;

namespace content {
class LocationArbitrator;

class CONTENT_EXPORT GeolocationProviderImpl
    : public NON_EXPORTED_BASE(GeolocationProvider),
      public base::Thread {
 public:
  // GeolocationProvider implementation:
  virtual scoped_ptr<GeolocationProvider::Subscription>
      AddLocationUpdateCallback(const LocationUpdateCallback& callback,
                                bool use_high_accuracy) OVERRIDE;
  virtual void UserDidOptIntoLocationServices() OVERRIDE;
  virtual void OverrideLocationForTesting(const Geoposition& position) OVERRIDE;

  // Callback from the LocationArbitrator. Public for testing.
  void OnLocationUpdate(const Geoposition& position);

  // Gets a pointer to the singleton instance of the location relayer, which
  // is in turn bound to the browser's global context objects. This must only be
  // called on the IO thread so that the GeolocationProviderImpl is always
  // instantiated on the same thread. Ownership is NOT returned.
  static GeolocationProviderImpl* GetInstance();

  bool user_did_opt_into_location_services_for_testing() {
    return user_did_opt_into_location_services_;
  }

 protected:
  friend struct DefaultSingletonTraits<GeolocationProviderImpl>;
  GeolocationProviderImpl();
  virtual ~GeolocationProviderImpl();

  // Useful for injecting mock geolocation arbitrator in tests.
  virtual LocationArbitrator* CreateArbitrator();

 private:
  bool OnGeolocationThread() const;

  // Start and stop providers as needed when clients are added or removed.
  void OnClientsChanged();

  // Stops the providers when there are no more registered clients. Note that
  // once the Geolocation thread is started, it will stay alive (but sitting
  // idle without any pending messages).
  void StopProviders();

  // Starts the geolocation providers or updates their options (delegates to
  // arbitrator).
  void StartProviders(bool use_high_accuracy);

  // Updates the providers on the geolocation thread, which must be running.
  void InformProvidersPermissionGranted();

  // Notifies all registered clients that a position update is available.
  void NotifyClients(const Geoposition& position);

  // Thread
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  base::CallbackList<void(const Geoposition&)> high_accuracy_callbacks_;
  base::CallbackList<void(const Geoposition&)> low_accuracy_callbacks_;

  bool user_did_opt_into_location_services_;
  Geoposition position_;

  // True only in testing, where we want to use a custom position.
  bool ignore_location_updates_;

  // Only to be used on the geolocation thread.
  LocationArbitrator* arbitrator_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
