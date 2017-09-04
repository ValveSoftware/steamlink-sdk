// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "components/physical_web/data_source/physical_web_data_source_impl.h"
#include "components/physical_web/data_source/physical_web_listener.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace physical_web {

// Test Values ----------------------------------------------------------------
const char kUrl[] = "https://www.google.com";

// TestPhysicalWebDataSource --------------------------------------------------

class TestPhysicalWebDataSource : public PhysicalWebDataSourceImpl {
 public:
  TestPhysicalWebDataSource() {}
  ~TestPhysicalWebDataSource() override {}

  void StartDiscovery(bool network_request_enabled) override;
  void StopDiscovery() override;
  std::unique_ptr<base::ListValue> GetMetadata() override;
  bool HasUnresolvedDiscoveries() override;
};
void TestPhysicalWebDataSource::StartDiscovery(bool network_request_enabled) {}

void TestPhysicalWebDataSource::StopDiscovery() {}

std::unique_ptr<base::ListValue> TestPhysicalWebDataSource::GetMetadata() {
  return NULL;
}

bool TestPhysicalWebDataSource::HasUnresolvedDiscoveries() {
  return false;
}

// TestPhysicalWebListener ----------------------------------------------------

class TestPhysicalWebListener : public PhysicalWebListener {
 public:
  TestPhysicalWebListener()
      : on_found_notified_(false),
        on_lost_notified_(false),
        on_distance_changed_notified_(false) {}

  ~TestPhysicalWebListener() {}

  void OnFound(const std::string& url) override {
    on_found_notified_ = true;
    last_event_url_ = url;
  }

  void OnLost(const std::string& url) override {
    on_lost_notified_ = true;
    last_event_url_ = url;
  }

  void OnDistanceChanged(const std::string& url,
                         double distance_estimate) override {
    on_distance_changed_notified_ = true;
    last_event_url_ = url;
  }

  bool OnFoundNotified() { return on_found_notified_; }

  bool OnLostNotified() { return on_lost_notified_; }

  bool OnDistanceChangedNotified() { return on_distance_changed_notified_; }

  std::string LastEventUrl() { return last_event_url_; }

 private:
  bool on_found_notified_;
  bool on_lost_notified_;
  bool on_distance_changed_notified_;
  std::string last_event_url_;
};

// PhysicalWebDataSourceImplTest ----------------------------------------------

class PhysicalWebDataSourceImplTest : public ::testing::Test {
 public:
  PhysicalWebDataSourceImplTest() {}
  ~PhysicalWebDataSourceImplTest() override {}

  // testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  TestPhysicalWebDataSource data_source_;
  TestPhysicalWebListener listener_;
};

void PhysicalWebDataSourceImplTest::SetUp() {
  data_source_.RegisterListener(&listener_);
}

void PhysicalWebDataSourceImplTest::TearDown() {
  data_source_.UnregisterListener(&listener_);
}

// Tests ----------------------------------------------------------------------

TEST_F(PhysicalWebDataSourceImplTest, OnFound) {
  data_source_.NotifyOnFound(kUrl);
  EXPECT_TRUE(listener_.OnFoundNotified());
  EXPECT_FALSE(listener_.OnLostNotified());
  EXPECT_FALSE(listener_.OnDistanceChangedNotified());
  EXPECT_EQ(kUrl, listener_.LastEventUrl());
}

TEST_F(PhysicalWebDataSourceImplTest, OnLost) {
  data_source_.NotifyOnLost(kUrl);
  EXPECT_FALSE(listener_.OnFoundNotified());
  EXPECT_TRUE(listener_.OnLostNotified());
  EXPECT_FALSE(listener_.OnDistanceChangedNotified());
  EXPECT_EQ(kUrl, listener_.LastEventUrl());
}

TEST_F(PhysicalWebDataSourceImplTest, OnDistanceChanged) {
  data_source_.NotifyOnDistanceChanged(kUrl, 0.0);
  EXPECT_FALSE(listener_.OnFoundNotified());
  EXPECT_FALSE(listener_.OnLostNotified());
  EXPECT_TRUE(listener_.OnDistanceChangedNotified());
  EXPECT_EQ(kUrl, listener_.LastEventUrl());
}

TEST_F(PhysicalWebDataSourceImplTest, OnFoundNotRegistered) {
  data_source_.UnregisterListener(&listener_);
  data_source_.NotifyOnFound(kUrl);
  EXPECT_FALSE(listener_.OnFoundNotified());
  EXPECT_FALSE(listener_.OnLostNotified());
  EXPECT_FALSE(listener_.OnDistanceChangedNotified());
  EXPECT_TRUE(listener_.LastEventUrl().empty());
}

TEST_F(PhysicalWebDataSourceImplTest, OnLostNotRegistered) {
  data_source_.UnregisterListener(&listener_);
  data_source_.NotifyOnLost(kUrl);
  EXPECT_FALSE(listener_.OnFoundNotified());
  EXPECT_FALSE(listener_.OnLostNotified());
  EXPECT_FALSE(listener_.OnDistanceChangedNotified());
  EXPECT_TRUE(listener_.LastEventUrl().empty());
}

TEST_F(PhysicalWebDataSourceImplTest, OnDistanceChangedNotRegistered) {
  data_source_.UnregisterListener(&listener_);
  data_source_.NotifyOnDistanceChanged(kUrl, 0.0);
  EXPECT_FALSE(listener_.OnFoundNotified());
  EXPECT_FALSE(listener_.OnLostNotified());
  EXPECT_FALSE(listener_.OnDistanceChangedNotified());
  EXPECT_TRUE(listener_.LastEventUrl().empty());
}
}  // namespace physical_web
