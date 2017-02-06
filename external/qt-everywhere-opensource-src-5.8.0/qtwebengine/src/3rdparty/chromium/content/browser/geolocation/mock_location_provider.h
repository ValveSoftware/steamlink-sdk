// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_


#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/location_provider_base.h"
#include "content/public/common/geoposition.h"

namespace content {

// Mock implementation of a location provider for testing.
class MockLocationProvider : public LocationProviderBase {
 public:
  enum State { STOPPED, LOW_ACCURACY, HIGH_ACCURACY } state_;

  MockLocationProvider();
  ~MockLocationProvider() override;

  // Updates listeners with the new position.
  void HandlePositionChanged(const Geoposition& position);

  // LocationProvider implementation.
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  void GetPosition(Geoposition* position) override;
  void OnPermissionGranted() override;

  bool is_permission_granted_;
  Geoposition position_;
  scoped_refptr<base::SingleThreadTaskRunner> provider_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLocationProvider);
};

// Factory functions for the various sorts of mock location providers,
// for use with LocationArbitrator::SetProviderFactoryForTest (i.e.
// not intended for test code to use to get access to the mock, you can use
// MockLocationProvider::instance_ for this, or make a custom factory method).

// Creates a mock location provider with no default behavior.
LocationProvider* NewMockLocationProvider();
// Creates a mock location provider that automatically notifies its
// listeners with a valid location when StartProvider is called.
LocationProvider* NewAutoSuccessMockLocationProvider();
// Creates a mock location provider that automatically notifies its
// listeners with an error when StartProvider is called.
LocationProvider* NewAutoFailMockLocationProvider();
// Similar to NewAutoSuccessMockLocationProvider but mimicks the behavior of
// the Network Location provider, in deferring making location updates until
// a permission request has been confirmed.
LocationProvider* NewAutoSuccessMockNetworkLocationProvider();

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
