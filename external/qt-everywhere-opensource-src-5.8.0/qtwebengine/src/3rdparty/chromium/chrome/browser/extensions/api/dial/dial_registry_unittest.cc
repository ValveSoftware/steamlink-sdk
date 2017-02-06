// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/browser/extensions/api/dial/dial_registry.h"
#include "chrome/browser/extensions/api/dial/dial_service.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;
using ::testing::A;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::InSequence;

namespace extensions {

class MockDialObserver : public DialRegistry::Observer {
 public:
  MOCK_METHOD1(OnDialDeviceEvent,
               void(const DialRegistry::DeviceList& devices));
  MOCK_METHOD1(OnDialError, void(DialRegistry::DialErrorCode type));
};

class MockDialService : public DialService {
 public:
  virtual ~MockDialService() {}

  MOCK_METHOD0(Discover, bool());
  MOCK_METHOD1(AddObserver, void(DialService::Observer*));
  MOCK_METHOD1(RemoveObserver, void(DialService::Observer*));
  MOCK_CONST_METHOD1(HasObserver, bool(const DialService::Observer*));
};

class MockDialRegistry : public DialRegistry {
 public:
  MockDialRegistry(Observer *dial_api,
                   const base::TimeDelta& refresh_interval,
                   const base::TimeDelta& expiration,
                   const size_t max_devices)
      : DialRegistry(dial_api, refresh_interval, expiration, max_devices) {
    time_ = Time::Now();
  }

  ~MockDialRegistry() override {
    // Don't let the DialRegistry delete this.
    DialService* tmp = dial_.release();
    if (tmp != NULL)
      CHECK_EQ(&mock_service_, tmp);
  }

  // Returns the mock Dial service.
  MockDialService& mock_service() {
    return mock_service_;
  }

  // Set to mock out the current time.
  Time time_;

 protected:
  base::Time Now() const override { return time_; }

  DialService* CreateDialService() override { return &mock_service_; }

  void ClearDialService() override {
    // Release the pointer but don't delete the object because the test owns it.
    CHECK_EQ(&mock_service_, dial_.release());
  }

 private:
  MockDialService mock_service_;
};

class DialRegistryTest : public testing::Test {
 public:
  DialRegistryTest()
      : first_device_("first", GURL("http://127.0.0.1/dd.xml"), Time::Now()),
        second_device_("second", GURL("http://127.0.0.2/dd.xml"), Time::Now()),
        third_device_("third", GURL("http://127.0.0.3/dd.xml"), Time::Now()) {
    registry_.reset(new MockDialRegistry(&mock_observer_,
                                         TimeDelta::FromSeconds(1000),
                                         TimeDelta::FromSeconds(10),
                                         10));
    list_with_first_device_.push_back(first_device_);
    list_with_second_device_.push_back(second_device_);
  }

 protected:
  std::unique_ptr<MockDialRegistry> registry_;
  MockDialObserver mock_observer_;
  const DialDeviceData first_device_;
  const DialDeviceData second_device_;
  const DialDeviceData third_device_;

  const DialRegistry::DeviceList empty_list_;
  DialRegistry::DeviceList list_with_first_device_;
  DialRegistry::DeviceList list_with_second_device_;

  // Must instantiate a MessageLoop for the thread, as the registry starts a
  // RepeatingTimer when there are listeners.
  base::MessageLoop message_loop_;

  void SetListenerExpectations() {
    EXPECT_CALL(registry_->mock_service(),
                AddObserver(A<DialService::Observer*>()));
    EXPECT_CALL(registry_->mock_service(),
                RemoveObserver(A<DialService::Observer*>()));
  }
};

TEST_F(DialRegistryTest, TestAddRemoveListeners) {
  SetListenerExpectations();
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_)).Times(2);

  EXPECT_FALSE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerAdded();
  EXPECT_TRUE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerAdded();
  EXPECT_TRUE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerRemoved();
  EXPECT_TRUE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerRemoved();
  EXPECT_FALSE(registry_->repeating_timer_.IsRunning());
}

TEST_F(DialRegistryTest, TestNoDevicesDiscovered) {
  SetListenerExpectations();
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(registry_->mock_service(), Discover());

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDevicesDiscovered) {
  DialRegistry::DeviceList expected_list2;
  expected_list2.push_back(first_device_);
  expected_list2.push_back(second_device_);

  SetListenerExpectations();
  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list2));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, second_device_);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDevicesDiscoveredWithTwoListeners) {
  DialRegistry::DeviceList expected_list2;
  expected_list2.push_back(first_device_);
  expected_list2.push_back(second_device_);

  SetListenerExpectations();
  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_))
      .Times(2);
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list2));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnListenerAdded();

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, second_device_);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDeviceExpires) {
  SetListenerExpectations();
  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->time_ = Time::Now() + TimeDelta::FromSeconds(30);

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestExpiredDeviceIsRediscovered) {
  std::vector<Time> discovery_times;
  discovery_times.push_back(Time::Now());
  discovery_times.push_back(discovery_times[0] + TimeDelta::FromSeconds(30));
  discovery_times.push_back(discovery_times[1] + TimeDelta::FromSeconds(30));

  DialDeviceData rediscovered_device("first",
                                     GURL("http://127.0.0.1/dd.xml"),
                                     discovery_times[2]);

  SetListenerExpectations();

  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));

  registry_->time_ = discovery_times[0];
  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  // Will expire "first" device as it is not discovered this time.
  registry_->time_ = discovery_times[1];
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);

  // "first" device is rediscovered 30 seconds later.  We pass a device object
  // with a newer discovery time so it is not pruned immediately.
  registry_->time_ = discovery_times[2];
  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, rediscovered_device);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestRemovingListenerDoesNotClearList) {
  DialRegistry::DeviceList expected_list2;
  expected_list2.push_back(first_device_);
  expected_list2.push_back(second_device_);

  InSequence s;
  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list2));
  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDeviceDiscovered(NULL, second_device_);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();

  // Removing and adding a listener again fires an event with the current device
  // list (even though no new devices were discovered).
  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()));
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list2));
  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestNetworkEventConnectionLost) {
  SetListenerExpectations();

  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));
  EXPECT_CALL(mock_observer_,
              OnDialError(DialRegistry::DIAL_NETWORK_DISCONNECTED));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnNetworkChanged(net::NetworkChangeNotifier::CONNECTION_NONE);

  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestNetworkEventConnectionRestored) {
  DialRegistry::DeviceList expected_list3;
  expected_list3.push_back(second_device_);
  expected_list3.push_back(third_device_);

  // A disconnection should shutdown the DialService, so we expect the observer
  // to be added twice.
  EXPECT_CALL(registry_->mock_service(),
              AddObserver(A<DialService::Observer*>()))
    .Times(2);
  EXPECT_CALL(registry_->mock_service(),
              RemoveObserver(A<DialService::Observer*>()))
    .Times(2);

  InSequence s;
  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_first_device_));

  EXPECT_CALL(mock_observer_,
              OnDialError(DialRegistry::DIAL_NETWORK_DISCONNECTED));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list_));

  EXPECT_CALL(registry_->mock_service(), Discover());
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(list_with_second_device_));
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list3));

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnNetworkChanged(net::NetworkChangeNotifier::CONNECTION_NONE);

  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnNetworkChanged(net::NetworkChangeNotifier::CONNECTION_WIFI);

  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, second_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnNetworkChanged(net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, third_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->OnListenerRemoved();
}

}  // namespace extensions
