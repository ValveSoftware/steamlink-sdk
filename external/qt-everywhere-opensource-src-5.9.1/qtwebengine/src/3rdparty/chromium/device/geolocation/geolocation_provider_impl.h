// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
#define DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_

#include <list>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/geolocation_provider.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
class SingleThreadTaskRunner;
}

namespace device {

class DEVICE_GEOLOCATION_EXPORT GeolocationProviderImpl
    : public NON_EXPORTED_BASE(GeolocationProvider),
      public base::Thread {
 public:
  // GeolocationProvider implementation:
  std::unique_ptr<GeolocationProvider::Subscription> AddLocationUpdateCallback(
      const LocationUpdateCallback& callback,
      bool enable_high_accuracy) override;
  void UserDidOptIntoLocationServices() override;
  void OverrideLocationForTesting(const Geoposition& position) override;

  // Callback from the LocationArbitrator. Public for testing.
  void OnLocationUpdate(const LocationProvider* provider,
                        const Geoposition& position);

  // Gets a pointer to the singleton instance of the location relayer, which
  // is in turn bound to the browser's global context objects. This must only be
  // called on the UI thread so that the GeolocationProviderImpl is always
  // instantiated on the same thread. Ownership is NOT returned.
  static GeolocationProviderImpl* GetInstance();

  bool user_did_opt_into_location_services_for_testing() {
    return user_did_opt_into_location_services_;
  }

  // Safe to call while there are no GeolocationProviderImpl clients
  // registered.
  void SetArbitratorForTesting(std::unique_ptr<LocationProvider> arbitrator);

 private:
  friend struct base::DefaultSingletonTraits<GeolocationProviderImpl>;
  GeolocationProviderImpl();
  ~GeolocationProviderImpl() override;

  bool OnGeolocationThread() const;

  // Start and stop providers as needed when clients are added or removed.
  void OnClientsChanged();

  // Stops the providers when there are no more registered clients. Note that
  // once the Geolocation thread is started, it will stay alive (but sitting
  // idle without any pending messages).
  void StopProviders();

  // Starts the geolocation providers or updates their options (delegates to
  // arbitrator).
  void StartProviders(bool enable_high_accuracy);

  // Updates the providers on the geolocation thread, which must be running.
  void InformProvidersPermissionGranted();

  // Notifies all registered clients that a position update is available.
  void NotifyClients(const Geoposition& position);

  // Thread
  void Init() override;
  void CleanUp() override;

  base::CallbackList<void(const Geoposition&)> high_accuracy_callbacks_;
  base::CallbackList<void(const Geoposition&)> low_accuracy_callbacks_;

  bool user_did_opt_into_location_services_;
  Geoposition position_;

  // True only in testing, where we want to use a custom position.
  bool ignore_location_updates_;

  // Used to PostTask()s from the geolocation thread to caller thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Only to be used on the geolocation thread.
  std::unique_ptr<LocationProvider> arbitrator_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationProviderImpl);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
