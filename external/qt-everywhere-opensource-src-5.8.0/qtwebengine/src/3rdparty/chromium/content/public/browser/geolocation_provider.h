// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GEOLOCATION_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_GEOLOCATION_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "content/common/content_export.h"

namespace content {
struct Geoposition;

// This is the main API to the geolocation subsystem. The application will hold
// a single instance of this class and can register multiple clients to be
// notified of location changes:
// * Callbacks are registered by AddLocationUpdateCallback() and will keep
//   receiving updates until the returned subscription object is destructed.
// The application must instantiate the GeolocationProvider on the UI thread and
// must communicate with it on the same thread.
// The underlying location arbitrator will only be enabled whilst there is at
// least one registered observer or pending callback (and only after
// UserDidOptIntoLocationServices). The arbitrator and the location providers it
// uses run on a separate Geolocation thread.
class GeolocationProvider {
 public:
  CONTENT_EXPORT static GeolocationProvider* GetInstance();

  typedef base::Callback<void(const Geoposition&)> LocationUpdateCallback;
  typedef base::CallbackList<void(const Geoposition&)>::Subscription
      Subscription;

  // |enable_high_accuracy| is used as a 'hint' for the provider preferences for
  // this particular observer, however the observer could receive updates for
  // best available locations from any active provider whilst it is registered.
  virtual std::unique_ptr<Subscription> AddLocationUpdateCallback(
      const LocationUpdateCallback& callback,
      bool enable_high_accuracy) = 0;

  // Calling this method indicates the user has opted into using location
  // services, including sending network requests to [Google servers to] resolve
  // the user's location. Use this method carefully, in line with the rules in
  // go/chrome-privacy-doc.
  virtual void UserDidOptIntoLocationServices() = 0;

  // Overrides the current location for testing.
  //
  // Overrides the location for automation/testing. Suppresses any further
  // updates from the actual providers and sends an update with the overridden
  // position to all registered clients.
  //
  // Do not use this function in unit tests. The function instantiates the
  // singleton geolocation stack in the background and manipulates it to report
  // a fake location. Neither step can be undone, breaking unit test isolation
  // (crbug.com/125931).
  virtual void OverrideLocationForTesting(const Geoposition& position) = 0;

 protected:
  virtual~GeolocationProvider() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GEOLOCATION_PROVIDER_H_
