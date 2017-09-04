// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/location_arbitrator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "device/geolocation/fake_access_token_store.h"
#include "device/geolocation/fake_location_provider.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/geoposition.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace device {
namespace {

class MockLocationObserver {
 public:
  virtual ~MockLocationObserver() {}
  void InvalidateLastPosition() {
    last_position_.latitude = 100;
    last_position_.error_code = Geoposition::ERROR_CODE_NONE;
    ASSERT_FALSE(last_position_.Validate());
  }
  void OnLocationUpdate(const LocationProvider* provider,
                        const Geoposition& position) {
    last_position_ = position;
  }

  Geoposition last_position() { return last_position_; }

 private:
  Geoposition last_position_;
};

double g_fake_time_now_secs = 1;

base::Time GetTimeNowForTest() {
  return base::Time::FromDoubleT(g_fake_time_now_secs);
}

void AdvanceTimeNow(const base::TimeDelta& delta) {
  g_fake_time_now_secs += delta.InSecondsF();
}

void SetPositionFix(FakeLocationProvider* provider,
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

// TODO(lethalantidote): Populate a Geoposition in the class from kConstants
// and then just copy that with "=" versus using a helper function.
void SetReferencePosition(FakeLocationProvider* provider) {
  SetPositionFix(provider, 51.0, -0.1, 400);
}

}  // namespace

class FakeGeolocationDelegate : public GeolocationDelegate {
 public:
  FakeGeolocationDelegate() = default;

  bool UseNetworkLocationProviders() override { return use_network_; }
  void set_use_network(bool use_network) { use_network_ = use_network; }

  std::unique_ptr<LocationProvider> OverrideSystemLocationProvider() override {
    DCHECK(!mock_location_provider_);
    mock_location_provider_ = new FakeLocationProvider;
    return base::WrapUnique(mock_location_provider_);
  }

  FakeLocationProvider* mock_location_provider() const {
    return mock_location_provider_;
  }

 private:
  bool use_network_ = true;
  FakeLocationProvider* mock_location_provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeGeolocationDelegate);
};

class TestingLocationArbitrator : public LocationArbitrator {
 public:
  TestingLocationArbitrator(
      const LocationProviderUpdateCallback& callback,
      const scoped_refptr<AccessTokenStore>& access_token_store,
      std::unique_ptr<GeolocationDelegate> delegate)
      : LocationArbitrator(std::move(delegate)),
        cell_(nullptr),
        gps_(nullptr),
        access_token_store_(access_token_store) {
    SetUpdateCallback(callback);
  }

  base::Time GetTimeNow() const override { return GetTimeNowForTest(); }

  scoped_refptr<AccessTokenStore> NewAccessTokenStore() override {
    return access_token_store_;
  }

  std::unique_ptr<LocationProvider> NewNetworkLocationProvider(
      const scoped_refptr<AccessTokenStore>& access_token_store,
      const scoped_refptr<net::URLRequestContextGetter>& context,
      const GURL& url,
      const base::string16& access_token) override {
    cell_ = new FakeLocationProvider;
    return base::WrapUnique(cell_);
  }

  std::unique_ptr<LocationProvider> NewSystemLocationProvider() override {
    gps_ = new FakeLocationProvider;
    return base::WrapUnique(gps_);
  }

  // Two location providers, with nice short names to make the tests more
  // readable. Note |gps_| will only be set when there is a high accuracy
  // observer registered (and |cell_| when there's at least one observer of any
  // type).
  // TODO(mvanouwerkerk): rename |cell_| to |network_location_provider_| and
  // |gps_| to |gps_location_provider_|
  FakeLocationProvider* cell_;
  FakeLocationProvider* gps_;
  const scoped_refptr<AccessTokenStore> access_token_store_;
};

class GeolocationLocationArbitratorTest : public testing::Test {
 protected:
  GeolocationLocationArbitratorTest()
      : access_token_store_(new NiceMock<FakeAccessTokenStore>),
        observer_(new MockLocationObserver),
        delegate_(new GeolocationDelegate) {}

  // Initializes |arbitrator_|, with the possibility of injecting a specific
  // |delegate|, otherwise a default, no-op GeolocationDelegate is used.
  void InitializeArbitrator(std::unique_ptr<GeolocationDelegate> delegate) {
    if (delegate)
      delegate_ = std::move(delegate);
    const LocationProvider::LocationProviderUpdateCallback callback =
        base::Bind(&MockLocationObserver::OnLocationUpdate,
                   base::Unretained(observer_.get()));
    arbitrator_.reset(new TestingLocationArbitrator(
        callback, access_token_store_, std::move(delegate_)));
  }

  // testing::Test
  void TearDown() override {}

  void CheckLastPositionInfo(double latitude,
                             double longitude,
                             double accuracy) {
    Geoposition geoposition = observer_->last_position();
    EXPECT_TRUE(geoposition.Validate());
    EXPECT_DOUBLE_EQ(latitude, geoposition.latitude);
    EXPECT_DOUBLE_EQ(longitude, geoposition.longitude);
    EXPECT_DOUBLE_EQ(accuracy, geoposition.accuracy);
  }

  base::TimeDelta SwitchOnFreshnessCliff() {
    // Add 1, to ensure it meets any greater-than test.
    return base::TimeDelta::FromMilliseconds(
        LocationArbitrator::kFixStaleTimeoutMilliseconds + 1);
  }

  FakeLocationProvider* cell() { return arbitrator_->cell_; }

  FakeLocationProvider* gps() { return arbitrator_->gps_; }

  const scoped_refptr<FakeAccessTokenStore> access_token_store_;
  const std::unique_ptr<MockLocationObserver> observer_;
  std::unique_ptr<TestingLocationArbitrator> arbitrator_;
  std::unique_ptr<GeolocationDelegate> delegate_;
  const base::MessageLoop loop_;
};

TEST_F(GeolocationLocationArbitratorTest, CreateDestroy) {
  EXPECT_TRUE(access_token_store_.get());
  InitializeArbitrator(nullptr);
  EXPECT_TRUE(arbitrator_);
  arbitrator_.reset();
  SUCCEED();
}

TEST_F(GeolocationLocationArbitratorTest, OnPermissionGranted) {
  InitializeArbitrator(nullptr);
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  // Can't check the provider has been notified without going through the
  // motions to create the provider (see next test).
  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
}

TEST_F(GeolocationLocationArbitratorTest, NormalUsage) {
  InitializeArbitrator(nullptr);
  ASSERT_TRUE(access_token_store_.get());
  ASSERT_TRUE(arbitrator_);

  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
  arbitrator_->StartProvider(false);

  EXPECT_TRUE(access_token_store_->access_token_map_.empty());

  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  EXPECT_TRUE(gps());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, gps()->state_);
  EXPECT_FALSE(observer_->last_position().Validate());
  EXPECT_EQ(Geoposition::ERROR_CODE_NONE,
            observer_->last_position().error_code);

  SetReferencePosition(cell());

  EXPECT_TRUE(observer_->last_position().Validate() ||
              observer_->last_position().error_code !=
                  Geoposition::ERROR_CODE_NONE);
  EXPECT_EQ(cell()->GetPosition().latitude,
            observer_->last_position().latitude);

  EXPECT_FALSE(cell()->is_permission_granted());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(cell()->is_permission_granted());
}

TEST_F(GeolocationLocationArbitratorTest, CustomSystemProviderOnly) {
  FakeGeolocationDelegate* fake_delegate = new FakeGeolocationDelegate;
  fake_delegate->set_use_network(false);

  std::unique_ptr<GeolocationDelegate> delegate(fake_delegate);
  InitializeArbitrator(std::move(delegate));
  ASSERT_TRUE(arbitrator_);

  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
  arbitrator_->StartProvider(false);

  ASSERT_FALSE(cell());
  EXPECT_FALSE(gps());
  ASSERT_TRUE(fake_delegate->mock_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY,
            fake_delegate->mock_location_provider()->state_);
  EXPECT_FALSE(observer_->last_position().Validate());
  EXPECT_EQ(Geoposition::ERROR_CODE_NONE,
            observer_->last_position().error_code);

  SetReferencePosition(fake_delegate->mock_location_provider());

  EXPECT_TRUE(observer_->last_position().Validate() ||
              observer_->last_position().error_code !=
                  Geoposition::ERROR_CODE_NONE);
  EXPECT_EQ(fake_delegate->mock_location_provider()->GetPosition().latitude,
            observer_->last_position().latitude);

  EXPECT_FALSE(
      fake_delegate->mock_location_provider()->is_permission_granted());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(fake_delegate->mock_location_provider()->is_permission_granted());
}

TEST_F(GeolocationLocationArbitratorTest,
       CustomSystemAndDefaultNetworkProviders) {
  FakeGeolocationDelegate* fake_delegate = new FakeGeolocationDelegate;
  fake_delegate->set_use_network(true);

  std::unique_ptr<GeolocationDelegate> delegate(fake_delegate);
  InitializeArbitrator(std::move(delegate));
  ASSERT_TRUE(arbitrator_);

  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
  arbitrator_->StartProvider(false);

  EXPECT_TRUE(access_token_store_->access_token_map_.empty());

  access_token_store_->NotifyDelegateTokensLoaded();

  ASSERT_TRUE(cell());
  EXPECT_FALSE(gps());
  ASSERT_TRUE(fake_delegate->mock_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY,
            fake_delegate->mock_location_provider()->state_);
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_FALSE(observer_->last_position().Validate());
  EXPECT_EQ(Geoposition::ERROR_CODE_NONE,
            observer_->last_position().error_code);

  SetReferencePosition(cell());

  EXPECT_TRUE(observer_->last_position().Validate() ||
              observer_->last_position().error_code !=
                  Geoposition::ERROR_CODE_NONE);
  EXPECT_EQ(cell()->GetPosition().latitude,
            observer_->last_position().latitude);

  EXPECT_FALSE(cell()->is_permission_granted());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(cell()->is_permission_granted());
}

TEST_F(GeolocationLocationArbitratorTest, SetObserverOptions) {
  InitializeArbitrator(nullptr);
  arbitrator_->StartProvider(false);
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, gps()->state_);
  SetReferencePosition(cell());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, gps()->state_);
  arbitrator_->StartProvider(true);
  EXPECT_EQ(FakeLocationProvider::HIGH_ACCURACY, cell()->state_);
  EXPECT_EQ(FakeLocationProvider::HIGH_ACCURACY, gps()->state_);
}

TEST_F(GeolocationLocationArbitratorTest, Arbitration) {
  InitializeArbitrator(nullptr);
  arbitrator_->StartProvider(false);
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());

  SetPositionFix(cell(), 1, 2, 150);

  // First position available
  EXPECT_TRUE(observer_->last_position().Validate());
  CheckLastPositionInfo(1, 2, 150);

  SetPositionFix(gps(), 3, 4, 50);

  // More accurate fix available
  CheckLastPositionInfo(3, 4, 50);

  SetPositionFix(cell(), 5, 6, 150);

  // New fix is available but it's less accurate, older fix should be kept.
  CheckLastPositionInfo(3, 4, 50);

  // Advance time, and notify once again
  AdvanceTimeNow(SwitchOnFreshnessCliff());
  cell()->HandlePositionChanged(cell()->GetPosition());

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
  InitializeArbitrator(nullptr);
  arbitrator_->StartProvider(false);
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());

  // Set the initial position.
  SetPositionFix(cell(), 3, 139, 100);
  CheckLastPositionInfo(3, 139, 100);

  // Restart providers to simulate a one-shot request.
  arbitrator_->StopProvider();

  // To test 240956, perform a throwaway alloc.
  // This convinces the allocator to put the providers in a new memory location.
  std::unique_ptr<FakeLocationProvider> dummy_provider(
      new FakeLocationProvider);

  arbitrator_->StartProvider(false);
  access_token_store_->NotifyDelegateTokensLoaded();

  // Advance the time a short while to simulate successive calls.
  AdvanceTimeNow(base::TimeDelta::FromMilliseconds(5));

  // Update with a less accurate position to verify 240956.
  SetPositionFix(cell(), 3, 139, 150);
  CheckLastPositionInfo(3, 139, 150);
}

}  // namespace device
