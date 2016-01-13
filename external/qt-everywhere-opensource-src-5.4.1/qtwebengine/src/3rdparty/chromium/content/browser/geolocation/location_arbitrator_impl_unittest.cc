// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/geolocation/fake_access_token_store.h"
#include "content/browser/geolocation/location_arbitrator_impl.h"
#include "content/browser/geolocation/mock_location_provider.h"
#include "content/public/common/geoposition.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace content {

class MockLocationObserver {
 public:
  // Need a vtable for GMock.
  virtual ~MockLocationObserver() {}
  void InvalidateLastPosition() {
    last_position_.latitude = 100;
    last_position_.error_code = Geoposition::ERROR_CODE_NONE;
    ASSERT_FALSE(last_position_.Validate());
  }
  // Delegate
  void OnLocationUpdate(const Geoposition& position) {
    last_position_ = position;
  }

  Geoposition last_position_;
};

double g_fake_time_now_secs = 1;

base::Time GetTimeNowForTest() {
  return base::Time::FromDoubleT(g_fake_time_now_secs);
}

void AdvanceTimeNow(const base::TimeDelta& delta) {
  g_fake_time_now_secs += delta.InSecondsF();
}

void SetPositionFix(MockLocationProvider* provider,
                    double latitude,
                    double longitude,
                    double accuracy) {
  Geoposition position;
  position.error_code = Geoposition::ERROR_CODE_NONE;
  position.latitude = latitude;
  position.longitude = longitude;
  position.accuracy = accuracy;
  position.timestamp = GetTimeNowForTest();
  ASSERT_TRUE(position.Validate());
  provider->HandlePositionChanged(position);
}

void SetReferencePosition(MockLocationProvider* provider) {
  SetPositionFix(provider, 51.0, -0.1, 400);
}

namespace {

class TestingLocationArbitrator : public LocationArbitratorImpl {
 public:
  TestingLocationArbitrator(
      const LocationArbitratorImpl::LocationUpdateCallback& callback,
      AccessTokenStore* access_token_store)
      : LocationArbitratorImpl(callback),
        cell_(NULL),
        gps_(NULL),
        access_token_store_(access_token_store) {
  }

  virtual base::Time GetTimeNow() const OVERRIDE {
    return GetTimeNowForTest();
  }

  virtual AccessTokenStore* NewAccessTokenStore() OVERRIDE {
    return access_token_store_.get();
  }

  virtual LocationProvider* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      net::URLRequestContextGetter* context,
      const GURL& url,
      const base::string16& access_token) OVERRIDE {
    return new MockLocationProvider(&cell_);
  }

  virtual LocationProvider* NewSystemLocationProvider() OVERRIDE {
    return new MockLocationProvider(&gps_);
  }

  // Two location providers, with nice short names to make the tests more
  // readable. Note |gps_| will only be set when there is a high accuracy
  // observer registered (and |cell_| when there's at least one observer of any
  // type).
  MockLocationProvider* cell_;
  MockLocationProvider* gps_;
  scoped_refptr<AccessTokenStore> access_token_store_;
};

}  // namespace

class GeolocationLocationArbitratorTest : public testing::Test {
 protected:
  // testing::Test
  virtual void SetUp() {
    access_token_store_ = new NiceMock<FakeAccessTokenStore>;
    observer_.reset(new MockLocationObserver);
    LocationArbitratorImpl::LocationUpdateCallback callback =
        base::Bind(&MockLocationObserver::OnLocationUpdate,
                   base::Unretained(observer_.get()));
    arbitrator_.reset(new TestingLocationArbitrator(
        callback, access_token_store_.get()));
  }

  // testing::Test
  virtual void TearDown() {
  }

  void CheckLastPositionInfo(double latitude,
                             double longitude,
                             double accuracy) {
    Geoposition geoposition = observer_->last_position_;
    EXPECT_TRUE(geoposition.Validate());
    EXPECT_DOUBLE_EQ(latitude, geoposition.latitude);
    EXPECT_DOUBLE_EQ(longitude, geoposition.longitude);
    EXPECT_DOUBLE_EQ(accuracy, geoposition.accuracy);
  }

  base::TimeDelta SwitchOnFreshnessCliff() {
    // Add 1, to ensure it meets any greater-than test.
    return base::TimeDelta::FromMilliseconds(
        LocationArbitratorImpl::kFixStaleTimeoutMilliseconds + 1);
  }

  MockLocationProvider* cell() {
    return arbitrator_->cell_;
  }

  MockLocationProvider* gps() {
    return arbitrator_->gps_;
  }

  scoped_refptr<FakeAccessTokenStore> access_token_store_;
  scoped_ptr<MockLocationObserver> observer_;
  scoped_ptr<TestingLocationArbitrator> arbitrator_;
  base::MessageLoop loop_;
};

TEST_F(GeolocationLocationArbitratorTest, CreateDestroy) {
  EXPECT_TRUE(access_token_store_.get());
  EXPECT_TRUE(arbitrator_ != NULL);
  arbitrator_.reset();
  SUCCEED();
}

TEST_F(GeolocationLocationArbitratorTest, OnPermissionGranted) {
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGranted());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGranted());
  // Can't check the provider has been notified without going through the
  // motions to create the provider (see next test).
  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
}

TEST_F(GeolocationLocationArbitratorTest, NormalUsage) {
  ASSERT_TRUE(access_token_store_.get());
  ASSERT_TRUE(arbitrator_ != NULL);

  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
  arbitrator_->StartProviders(false);

  EXPECT_TRUE(access_token_store_->access_token_set_.empty());
  EXPECT_TRUE(access_token_store_->access_token_set_.empty());

  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  EXPECT_TRUE(gps());
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
  EXPECT_FALSE(observer_->last_position_.Validate());
  EXPECT_EQ(Geoposition::ERROR_CODE_NONE,
            observer_->last_position_.error_code);

  SetReferencePosition(cell());

  EXPECT_TRUE(observer_->last_position_.Validate() ||
              observer_->last_position_.error_code !=
                  Geoposition::ERROR_CODE_NONE);
  EXPECT_EQ(cell()->position_.latitude,
            observer_->last_position_.latitude);

  EXPECT_FALSE(cell()->is_permission_granted_);
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGranted());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGranted());
  EXPECT_TRUE(cell()->is_permission_granted_);
}

TEST_F(GeolocationLocationArbitratorTest, SetObserverOptions) {
  arbitrator_->StartProviders(false);
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
  SetReferencePosition(cell());
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
  arbitrator_->StartProviders(true);
  EXPECT_EQ(MockLocationProvider::HIGH_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::HIGH_ACCURACY, gps()->state_);
}

TEST_F(GeolocationLocationArbitratorTest, Arbitration) {
  arbitrator_->StartProviders(false);
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());

  SetPositionFix(cell(), 1, 2, 150);

  // First position available
  EXPECT_TRUE(observer_->last_position_.Validate());
  CheckLastPositionInfo(1, 2, 150);

  SetPositionFix(gps(), 3, 4, 50);

  // More accurate fix available
  CheckLastPositionInfo(3, 4, 50);

  SetPositionFix(cell(), 5, 6, 150);

  // New fix is available but it's less accurate, older fix should be kept.
  CheckLastPositionInfo(3, 4, 50);

  // Advance time, and notify once again
  AdvanceTimeNow(SwitchOnFreshnessCliff());
  cell()->HandlePositionChanged(cell()->position_);

  // New fix is available, less accurate but fresher
  CheckLastPositionInfo(5, 6, 150);

  // Advance time, and set a low accuracy position
  AdvanceTimeNow(SwitchOnFreshnessCliff());
  SetPositionFix(cell(), 5.676731, 139.629385, 1000);
  CheckLastPositionInfo(5.676731, 139.629385, 1000);

  // 15 secs later, step outside. Switches to gps signal.
  AdvanceTimeNow(base::TimeDelta::FromSeconds(15));
  SetPositionFix(gps(), 3.5676457, 139.629198, 50);
  CheckLastPositionInfo(3.5676457, 139.629198, 50);

  // 5 mins later switch cells while walking. Stay on gps.
  AdvanceTimeNow(base::TimeDelta::FromMinutes(5));
  SetPositionFix(cell(), 3.567832, 139.634648, 300);
  SetPositionFix(gps(), 3.5677675, 139.632314, 50);
  CheckLastPositionInfo(3.5677675, 139.632314, 50);

  // Ride train and gps signal degrades slightly. Stay on fresher gps
  AdvanceTimeNow(base::TimeDelta::FromMinutes(5));
  SetPositionFix(gps(), 3.5679026, 139.634777, 300);
  CheckLastPositionInfo(3.5679026, 139.634777, 300);

  // 14 minutes later
  AdvanceTimeNow(base::TimeDelta::FromMinutes(14));

  // GPS reading misses a beat, but don't switch to cell yet to avoid
  // oscillating.
  SetPositionFix(gps(), 3.5659005, 139.682579, 300);

  AdvanceTimeNow(base::TimeDelta::FromSeconds(7));
  SetPositionFix(cell(), 3.5689579, 139.691420, 1000);
  CheckLastPositionInfo(3.5659005, 139.682579, 300);

  // 1 minute later
  AdvanceTimeNow(base::TimeDelta::FromMinutes(1));

  // Enter tunnel. Stay on fresher gps for a moment.
  SetPositionFix(cell(), 3.5657078, 139.68922, 300);
  SetPositionFix(gps(), 3.5657104, 139.690341, 300);
  CheckLastPositionInfo(3.5657104, 139.690341, 300);

  // 2 minutes later
  AdvanceTimeNow(base::TimeDelta::FromMinutes(2));
  // Arrive in station. Cell moves but GPS is stale. Switch to fresher cell.
  SetPositionFix(cell(), 3.5658700, 139.069979, 1000);
  CheckLastPositionInfo(3.5658700, 139.069979, 1000);
}

TEST_F(GeolocationLocationArbitratorTest, TwoOneShotsIsNewPositionBetter) {
  arbitrator_->StartProviders(false);
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());

  // Set the initial position.
  SetPositionFix(cell(), 3, 139, 100);
  CheckLastPositionInfo(3, 139, 100);

  // Restart providers to simulate a one-shot request.
  arbitrator_->StopProviders();

  // To test 240956, perform a throwaway alloc.
  // This convinces the allocator to put the providers in a new memory location.
  MockLocationProvider* fakeMockProvider = NULL;
  LocationProvider* fakeProvider =
      new MockLocationProvider(&fakeMockProvider);

  arbitrator_->StartProviders(false);
  access_token_store_->NotifyDelegateTokensLoaded();

  // Advance the time a short while to simulate successive calls.
  AdvanceTimeNow(base::TimeDelta::FromMilliseconds(5));

  // Update with a less accurate position to verify 240956.
  SetPositionFix(cell(), 3, 139, 150);
  CheckLastPositionInfo(3, 139, 150);

  // No delete required for fakeMockProvider. It points to fakeProvider.
  delete fakeProvider;
}

}  // namespace content
