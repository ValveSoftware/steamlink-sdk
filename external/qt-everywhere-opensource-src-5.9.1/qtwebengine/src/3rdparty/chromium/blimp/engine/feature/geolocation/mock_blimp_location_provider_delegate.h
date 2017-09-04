// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_GEOLOCATION_MOCK_BLIMP_LOCATION_PROVIDER_DELEGATE_H_
#define BLIMP_ENGINE_FEATURE_GEOLOCATION_MOCK_BLIMP_LOCATION_PROVIDER_DELEGATE_H_

#include "blimp/engine/feature/geolocation/blimp_location_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace engine {

class MockBlimpLocationProviderDelegate
    : public BlimpLocationProvider::Delegate {
 public:
  MockBlimpLocationProviderDelegate();
  ~MockBlimpLocationProviderDelegate();

  base::WeakPtr<MockBlimpLocationProviderDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD1(RequestAccuracy,
               void(GeolocationSetInterestLevelMessage::Level level));
  MOCK_METHOD0(OnPermissionGranted, void());
  MOCK_METHOD1(SetUpdateCallback,
               void(const GeopositionReceivedCallback& callback));

 private:
  base::WeakPtrFactory<MockBlimpLocationProviderDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockBlimpLocationProviderDelegate);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_GEOLOCATION_MOCK_BLIMP_LOCATION_PROVIDER_DELEGATE_H_
