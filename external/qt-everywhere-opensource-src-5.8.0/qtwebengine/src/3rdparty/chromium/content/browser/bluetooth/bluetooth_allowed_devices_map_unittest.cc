// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using device::BluetoothUUID;

namespace content {
namespace {
const url::Origin kTestOrigin1(GURL("https://www.example1.com"));
const url::Origin kTestOrigin2(GURL("https://www.example2.com"));

const std::string kDeviceAddress1 = "00:00:00";
const std::string kDeviceAddress2 = "11:11:11";

const char kGlucoseUUIDString[] = "00001808-0000-1000-8000-00805f9b34fb";
const char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";
const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
const char kBloodPressureUUIDString[] = "00001813-0000-1000-8000-00805f9b34fb";
const char kCyclingPowerUUIDString[] = "00001818-0000-1000-8000-00805f9b34fb";
const BluetoothUUID kGlucoseUUID(kGlucoseUUIDString);
const BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
const BluetoothUUID kBatteryServiceUUID(kBatteryServiceUUIDString);
const BluetoothUUID kBloodPressureUUID(kBloodPressureUUIDString);
const BluetoothUUID kCyclingPowerUUID(kCyclingPowerUUIDString);

class BluetoothAllowedDevicesMapTest : public testing::Test {
 protected:
  BluetoothAllowedDevicesMapTest() {
    empty_options_ = blink::mojom::WebBluetoothRequestDeviceOptions::New();
  }

  ~BluetoothAllowedDevicesMapTest() override {}

  blink::mojom::WebBluetoothRequestDeviceOptionsPtr empty_options_;
};

}  // namespace

TEST_F(BluetoothAllowedDevicesMapTest, UniqueOriginNotSupported) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  EXPECT_DEATH_IF_SUPPORTED(allowed_devices_map.AddDevice(
                                url::Origin(), kDeviceAddress1, empty_options_),
                            "");
}

TEST_F(BluetoothAllowedDevicesMapTest, AddDeviceToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddDeviceToMapTwice) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);

  EXPECT_EQ(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id1));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddTwoDevicesFromSameOriginToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress2, empty_options_);

  EXPECT_NE(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(device_id2,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress2));

  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id1));
  EXPECT_EQ(kDeviceAddress2,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id2));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddTwoDevicesFromTwoOriginsToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin2, kDeviceAddress2, empty_options_);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(kTestOrigin2, kDeviceAddress1));

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin2, device_id1));

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(device_id2,
            allowed_devices_map.GetDeviceId(kTestOrigin2, kDeviceAddress2));

  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id1));
  EXPECT_EQ(kDeviceAddress2,
            allowed_devices_map.GetDeviceAddress(kTestOrigin2, device_id2));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddDeviceFromTwoOriginsToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin2, kDeviceAddress1, empty_options_);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin2, device_id1));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddRemoveAddDeviceToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string device_id_first_time = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);

  allowed_devices_map.RemoveDevice(kTestOrigin1, kDeviceAddress1);

  const std::string device_id_second_time = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);

  EXPECT_NE(device_id_first_time, device_id_second_time);
}

TEST_F(BluetoothAllowedDevicesMapTest, RemoveDeviceFromMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);

  allowed_devices_map.RemoveDevice(kTestOrigin1, kDeviceAddress1);

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(kTestOrigin1, device_id));
  EXPECT_EQ(base::EmptyString(), allowed_devices_map.GetDeviceAddress(
                                     kTestOrigin1, kDeviceAddress1));
}

TEST_F(BluetoothAllowedDevicesMapTest, AllowedServices_OneOriginOneDevice) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  // Setup device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter1 =
      blink::mojom::WebBluetoothScanFilter::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter2 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter1->services.push_back(kGlucoseUUID);
  options->filters.push_back(scanFilter1.Clone());

  scanFilter2->services.push_back(kHeartRateUUID);
  options->filters.push_back(scanFilter2.Clone());

  options->optional_services.push_back(kBatteryServiceUUID);
  options->optional_services.push_back(kHeartRateUUID);

  // Add to map.
  const std::string device_id1 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress1, options);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUID));

  // Try to access a non-allowed service.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUID));

  // Try to access allowed services after removing device.
  allowed_devices_map.RemoveDevice(kTestOrigin1, kDeviceAddress1);

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUID));

  // Add device back.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();

  options2->filters.push_back(scanFilter1.Clone());
  options2->filters.push_back(scanFilter2.Clone());

  const std::string device_id2 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress1, options2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kGlucoseUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kHeartRateUUID));

  // Try to access a non-allowed service.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBatteryServiceUUID));

  // Try to access services from old device.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUID));
}

TEST_F(BluetoothAllowedDevicesMapTest, AllowedServices_OneOriginTwoDevices) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  // Setup request for device #1.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options1 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter1 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter1->services.push_back(kGlucoseUUID);
  options1->filters.push_back(std::move(scanFilter1));

  options1->optional_services.push_back(kHeartRateUUID);

  // Setup request for device #2.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter2 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter2->services.push_back(kBatteryServiceUUID);
  options2->filters.push_back(std::move(scanFilter2));

  options2->optional_services.push_back(kBloodPressureUUID);

  // Add devices to map.
  const std::string& device_id1 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress1, options1);
  const std::string& device_id2 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress2, options2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUID));

  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBatteryServiceUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBloodPressureUUID));

  // Try to access non-allowed services.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kCyclingPowerUUID));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kGlucoseUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kCyclingPowerUUID));
}

TEST_F(BluetoothAllowedDevicesMapTest, AllowedServices_TwoOriginsOneDevice) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  // Setup request #1 for device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options1 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter1 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter1->services.push_back(kGlucoseUUID);
  options1->filters.push_back(std::move(scanFilter1));

  options1->optional_services.push_back(kHeartRateUUID);

  // Setup request #2 for device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter2 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter2->services.push_back(kBatteryServiceUUID);
  options2->filters.push_back(std::move(scanFilter2));

  options2->optional_services.push_back(kBloodPressureUUID);

  // Add devices to map.
  const std::string& device_id1 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress1, options1);
  const std::string& device_id2 =
      allowed_devices_map.AddDevice(kTestOrigin2, kDeviceAddress1, options2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUID));

  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kBatteryServiceUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kBloodPressureUUID));

  // Try to access non-allowed services.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUID));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kGlucoseUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBloodPressureUUID));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kGlucoseUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kHeartRateUUID));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kGlucoseUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kBloodPressureUUID));
}

TEST_F(BluetoothAllowedDevicesMapTest, MergeServices) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  // Setup first request.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options1 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter1 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter1->services.push_back(kGlucoseUUID);
  options1->filters.push_back(std::move(scanFilter1));

  options1->optional_services.push_back(kBatteryServiceUUID);

  // Add to map.
  const std::string device_id1 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress1, options1);

  // Setup second request.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothScanFilterPtr scanFilter2 =
      blink::mojom::WebBluetoothScanFilter::New();

  scanFilter2->services.push_back(kHeartRateUUID);
  options2->filters.push_back(std::move(scanFilter2));

  options2->optional_services.push_back(kBloodPressureUUID);

  // Add to map again.
  const std::string device_id2 =
      allowed_devices_map.AddDevice(kTestOrigin1, kDeviceAddress1, options2);

  EXPECT_EQ(device_id1, device_id2);

  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUID));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUID));
}

TEST_F(BluetoothAllowedDevicesMapTest, CorrectIdFormat) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, empty_options_);

  EXPECT_TRUE(device_id.size() == 24)
      << "Expected Lenghth of a 128bit string encoded to Base64.";
  EXPECT_TRUE((device_id[22] == '=') && (device_id[23] == '='))
      << "Expected padding characters for a 128bit string encoded to Base64.";
}

}  // namespace content
