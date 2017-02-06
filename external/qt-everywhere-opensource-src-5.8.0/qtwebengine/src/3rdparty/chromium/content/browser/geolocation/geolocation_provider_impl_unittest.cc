// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_provider_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/geolocation/mock_location_arbitrator.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;

namespace content {

class LocationProviderForTestArbitrator : public GeolocationProviderImpl {
 public:
  LocationProviderForTestArbitrator() : mock_arbitrator_(NULL) {}
  ~LocationProviderForTestArbitrator() override {}

  // Only valid for use on the geolocation thread.
  MockLocationArbitrator* mock_arbitrator() const {
    return mock_arbitrator_;
  }

 protected:
  // GeolocationProviderImpl implementation:
  std::unique_ptr<LocationArbitrator> CreateArbitrator() override;

 private:
  // An alias to the arbitrator stored in the super class, where it is owned.
  MockLocationArbitrator* mock_arbitrator_;
};

std::unique_ptr<LocationArbitrator>
LocationProviderForTestArbitrator::CreateArbitrator() {
  DCHECK(mock_arbitrator_ == NULL);
  mock_arbitrator_ = new MockLocationArbitrator;
  return base::WrapUnique(mock_arbitrator_);
}

class GeolocationObserver {
 public:
  virtual ~GeolocationObserver() {}
  virtual void OnLocationUpdate(const Geoposition& position) = 0;
};

class MockGeolocationObserver : public GeolocationObserver {
 public:
  MOCK_METHOD1(OnLocationUpdate, void(const Geoposition& position));
};

class AsyncMockGeolocationObserver : public MockGeolocationObserver {
 public:
  void OnLocationUpdate(const Geoposition& position) override {
    MockGeolocationObserver::OnLocationUpdate(position);
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

class MockGeolocationCallbackWrapper {
 public:
  MOCK_METHOD1(Callback, void(const Geoposition& position));
};

class GeopositionEqMatcher
    : public MatcherInterface<const Geoposition&> {
 public:
  explicit GeopositionEqMatcher(const Geoposition& expected)
      : expected_(expected) {}

  bool MatchAndExplain(const Geoposition& actual,
                       MatchResultListener* listener) const override {
    return actual.latitude == expected_.latitude &&
           actual.longitude == expected_.longitude &&
           actual.altitude == expected_.altitude &&
           actual.accuracy == expected_.accuracy &&
           actual.altitude_accuracy == expected_.altitude_accuracy &&
           actual.heading == expected_.heading &&
           actual.speed == expected_.speed &&
           actual.timestamp == expected_.timestamp &&
           actual.error_code == expected_.error_code &&
           actual.error_message == expected_.error_message;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "which matches the expected position";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "which does not match the expected position";
  }

 private:
  Geoposition expected_;

  DISALLOW_COPY_AND_ASSIGN(GeopositionEqMatcher);
};

Matcher<const Geoposition&> GeopositionEq(const Geoposition& expected) {
  return MakeMatcher(new GeopositionEqMatcher(expected));
}

class GeolocationProviderTest : public testing::Test {
 protected:
  GeolocationProviderTest()
      : message_loop_(),
        ui_thread_(BrowserThread::UI, &message_loop_),
        provider_(new LocationProviderForTestArbitrator) {
  }

  ~GeolocationProviderTest() override {}

  LocationProviderForTestArbitrator* provider() { return provider_.get(); }

  // Called on test thread.
  bool ProvidersStarted();
  void SendMockLocation(const Geoposition& position);

 private:
  // Called on provider thread.
  void GetProvidersStarted(bool* started);

  base::MessageLoop message_loop_;
  TestBrowserThread ui_thread_;
  std::unique_ptr<LocationProviderForTestArbitrator> provider_;
};


bool GeolocationProviderTest::ProvidersStarted() {
  DCHECK(provider_->IsRunning());
  DCHECK(base::MessageLoop::current() == &message_loop_);
  bool started;
  provider_->task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&GeolocationProviderTest::GetProvidersStarted,
                            base::Unretained(this), &started),
      base::MessageLoop::QuitWhenIdleClosure());
  message_loop_.Run();
  return started;
}

void GeolocationProviderTest::GetProvidersStarted(bool* started) {
  DCHECK(base::MessageLoop::current() == provider_->message_loop());
  *started = provider_->mock_arbitrator()->providers_started();
}

void GeolocationProviderTest::SendMockLocation(const Geoposition& position) {
  DCHECK(provider_->IsRunning());
  DCHECK(base::MessageLoop::current() == &message_loop_);
  provider_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GeolocationProviderImpl::OnLocationUpdate,
                            base::Unretained(provider_.get()), position));
}

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->UserDidOptIntoLocationServices();
  EXPECT_TRUE(provider()->user_did_opt_into_location_services_for_testing());
}

void DummyFunction(const Geoposition& position) {
}

TEST_F(GeolocationProviderTest, StartStop) {
  EXPECT_FALSE(provider()->IsRunning());
  GeolocationProviderImpl::LocationUpdateCallback callback =
      base::Bind(&DummyFunction);
  std::unique_ptr<content::GeolocationProvider::Subscription> subscription =
      provider()->AddLocationUpdateCallback(callback, false);
  EXPECT_TRUE(provider()->IsRunning());
  EXPECT_TRUE(ProvidersStarted());

  subscription.reset();

  EXPECT_FALSE(ProvidersStarted());
  EXPECT_TRUE(provider()->IsRunning());
}

TEST_F(GeolocationProviderTest, StalePositionNotSent) {
  Geoposition first_position;
  first_position.latitude = 12;
  first_position.longitude = 34;
  first_position.accuracy = 56;
  first_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver first_observer;
  GeolocationProviderImpl::LocationUpdateCallback first_callback = base::Bind(
      &MockGeolocationObserver::OnLocationUpdate,
      base::Unretained(&first_observer));
  EXPECT_CALL(first_observer, OnLocationUpdate(GeopositionEq(first_position)));
  std::unique_ptr<content::GeolocationProvider::Subscription> subscription =
      provider()->AddLocationUpdateCallback(first_callback, false);
  SendMockLocation(first_position);
  base::MessageLoop::current()->Run();

  subscription.reset();

  Geoposition second_position;
  second_position.latitude = 13;
  second_position.longitude = 34;
  second_position.accuracy = 56;
  second_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver second_observer;

  // After adding a second observer, check that no unexpected position update
  // is sent.
  EXPECT_CALL(second_observer, OnLocationUpdate(testing::_)).Times(0);
  GeolocationProviderImpl::LocationUpdateCallback second_callback = base::Bind(
      &MockGeolocationObserver::OnLocationUpdate,
      base::Unretained(&second_observer));
  std::unique_ptr<content::GeolocationProvider::Subscription> subscription2 =
      provider()->AddLocationUpdateCallback(second_callback, false);
  base::MessageLoop::current()->RunUntilIdle();

  // The second observer should receive the new position now.
  EXPECT_CALL(second_observer,
              OnLocationUpdate(GeopositionEq(second_position)));
  SendMockLocation(second_position);
  base::MessageLoop::current()->Run();

  subscription2.reset();
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  Geoposition position;
  position.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  provider()->OverrideLocationForTesting(position);
  // Adding an observer when the location is overridden should synchronously
  // update the observer with our overridden position.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnLocationUpdate(GeopositionEq(position)));
  GeolocationProviderImpl::LocationUpdateCallback callback = base::Bind(
      &MockGeolocationObserver::OnLocationUpdate,
      base::Unretained(&mock_observer));
  std::unique_ptr<content::GeolocationProvider::Subscription> subscription =
      provider()->AddLocationUpdateCallback(callback, false);
  subscription.reset();
  // Wait for the providers to be stopped now that all clients are gone.
  EXPECT_FALSE(ProvidersStarted());
}

}  // namespace content
