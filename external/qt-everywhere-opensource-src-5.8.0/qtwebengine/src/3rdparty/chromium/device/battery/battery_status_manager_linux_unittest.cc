// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_status_manager_linux.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

TEST(BatteryStatusManagerLinuxTest, EmptyDictionary) {
  base::DictionaryValue dictionary;
  BatteryStatus default_status;
  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_EQ(default_status.charging, status.charging);
  EXPECT_EQ(default_status.charging_time, status.charging_time);
  EXPECT_EQ(default_status.discharging_time, status.discharging_time);
  EXPECT_EQ(default_status.level, status.level);
}

TEST(BatteryStatusManagerLinuxTest, ChargingHalfFull) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_CHARGING);
  dictionary.SetDouble("TimeToFull", 0);
  dictionary.SetDouble("Percentage", 50);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.5, status.level);
}

TEST(BatteryStatusManagerLinuxTest, ChargingTimeToFull) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_CHARGING);
  dictionary.SetDouble("TimeToFull", 100.f);
  dictionary.SetDouble("Percentage", 1);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.01, status.level);
}

TEST(BatteryStatusManagerLinuxTest, FullyCharged) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_FULL);
  dictionary.SetDouble("TimeToFull", 100);
  dictionary.SetDouble("TimeToEmpty", 200);
  dictionary.SetDouble("Percentage", 100);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(0, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerLinuxTest, Discharging) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_DISCHARGING);
  dictionary.SetDouble("TimeToFull", 0);
  dictionary.SetDouble("TimeToEmpty", 200);
  dictionary.SetDouble("Percentage", 90);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST(BatteryStatusManagerLinuxTest, DischargingTimeToEmptyUnknown) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_DISCHARGING);
  dictionary.SetDouble("TimeToFull", 0);
  dictionary.SetDouble("TimeToEmpty", 0);
  dictionary.SetDouble("Percentage", 90);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST(BatteryStatusManagerLinuxTest, DeviceStateUnknown) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_UNKNOWN);
  dictionary.SetDouble("TimeToFull", 0);
  dictionary.SetDouble("TimeToEmpty", 0);
  dictionary.SetDouble("Percentage", 50);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.5, status.level);
}

TEST(BatteryStatusManagerLinuxTest, DeviceStateEmpty) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_EMPTY);
  dictionary.SetDouble("TimeToFull", 0);
  dictionary.SetDouble("TimeToEmpty", 0);
  dictionary.SetDouble("Percentage", 0);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0, status.level);
}

TEST(BatteryStatusManagerLinuxTest, LevelRoundedToThreeSignificantDigits) {
  base::DictionaryValue dictionary;
  dictionary.SetDouble("State", UPOWER_DEVICE_STATE_DISCHARGING);
  dictionary.SetDouble("Percentage", 14.56);

  BatteryStatus status = ComputeWebBatteryStatus(dictionary);

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.15, status.level);
}

}  // namespace

}  // namespace device
