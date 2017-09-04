// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaTransferProtocolDeviceObserverChromeOS unit tests.

#include "components/storage_monitor/media_transfer_protocol_device_observer_chromeos.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

namespace {

// Sample mtp device storage information.
const char kStorageLabel[] = "Camera V1.0";
const char kStorageLocation[] = "/usb:2,2,88888";
const char kStorageUniqueId[] = "VendorModelSerial:COM:MOD2012:283";
const char kStorageWithInvalidInfo[] = "usb:2,3,11111";
const char kStorageWithValidInfo[] = "usb:2,2,88888";
const char kStorageVendor[] = "ExampleVendor";
const char kStorageProductName[] = "ExampleCamera";

// Returns the mtp device id given the |unique_id|.
std::string GetMtpDeviceId(const std::string& unique_id) {
  return StorageInfo::MakeDeviceId(StorageInfo::MTP_OR_PTP, unique_id);
}

// Helper function to get the device storage details such as device id, label
// and location. On success, fills in |id|, |label|, |location|, |vendor_name|,
// and |product_name|.
void GetStorageInfo(const std::string& storage_name,
                    device::MediaTransferProtocolManager* mtp_manager,
                    std::string* id,
                    base::string16* label,
                    std::string* location,
                    base::string16* vendor_name,
                    base::string16* product_name) {
  if (storage_name == kStorageWithInvalidInfo)
    return;  // Do not set any storage details.

  ASSERT_EQ(kStorageWithValidInfo, storage_name);

  *id = GetMtpDeviceId(kStorageUniqueId);
  *label = base::ASCIIToUTF16(kStorageLabel);
  *location = kStorageLocation;
  *vendor_name = base::ASCIIToUTF16(kStorageVendor);
  *product_name = base::ASCIIToUTF16(kStorageProductName);
}

class TestMediaTransferProtocolDeviceObserverChromeOS
    : public MediaTransferProtocolDeviceObserverChromeOS {
 public:
  TestMediaTransferProtocolDeviceObserverChromeOS(
      StorageMonitor::Receiver* receiver,
      device::MediaTransferProtocolManager* mtp_manager)
      : MediaTransferProtocolDeviceObserverChromeOS(receiver,
                                                    mtp_manager,
                                                    &GetStorageInfo) {}

  // Notifies MediaTransferProtocolDeviceObserverChromeOS about the attachment
  // of
  // mtp storage device given the |storage_name|.
  void MtpStorageAttached(const std::string& storage_name) {
    StorageChanged(true, storage_name);
    base::RunLoop().RunUntilIdle();
  }

  // Notifies MediaTransferProtocolDeviceObserverChromeOS about the detachment
  // of
  // mtp storage device given the |storage_name|.
  void MtpStorageDetached(const std::string& storage_name) {
    StorageChanged(false, storage_name);
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMediaTransferProtocolDeviceObserverChromeOS);
};

}  // namespace

// A class to test the functionality of
// MediaTransferProtocolDeviceObserverChromeOS member functions.
class MediaTransferProtocolDeviceObserverChromeOSTest : public testing::Test {
 public:
  MediaTransferProtocolDeviceObserverChromeOSTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  ~MediaTransferProtocolDeviceObserverChromeOSTest() override {}

 protected:
  void SetUp() override {
    mock_storage_observer_.reset(new MockRemovableStorageObserver);
    TestStorageMonitor* monitor = TestStorageMonitor::CreateAndInstall();
    mtp_device_observer_.reset(
        new TestMediaTransferProtocolDeviceObserverChromeOS(
            monitor->receiver(), monitor->media_transfer_protocol_manager()));
    monitor->AddObserver(mock_storage_observer_.get());
  }

  void TearDown() override {
    StorageMonitor* monitor = StorageMonitor::GetInstance();
    monitor->RemoveObserver(mock_storage_observer_.get());
    mtp_device_observer_.reset();
    TestStorageMonitor::Destroy();
  }

  // Returns the device changed observer object.
  MockRemovableStorageObserver& observer() { return *mock_storage_observer_; }

  TestMediaTransferProtocolDeviceObserverChromeOS* mtp_device_observer() {
    return mtp_device_observer_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestMediaTransferProtocolDeviceObserverChromeOS>
      mtp_device_observer_;
  std::unique_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverChromeOSTest);
};

// Test to verify basic mtp storage attach and detach notifications.
TEST_F(MediaTransferProtocolDeviceObserverChromeOSTest, BasicAttachDetach) {
  std::string device_id = GetMtpDeviceId(kStorageUniqueId);

  // Attach a mtp storage.
  mtp_device_observer()->MtpStorageAttached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_attached().device_id());
  EXPECT_EQ(kStorageLocation, observer().last_attached().location());
  EXPECT_EQ(base::ASCIIToUTF16(kStorageVendor),
            observer().last_attached().vendor_name());
  EXPECT_EQ(base::ASCIIToUTF16(kStorageProductName),
            observer().last_attached().model_name());

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_detached().device_id());
}

// When a mtp storage device with invalid storage label and id is
// attached/detached, there should not be any device attach/detach
// notifications.
TEST_F(MediaTransferProtocolDeviceObserverChromeOSTest,
       StorageWithInvalidInfo) {
  // Attach the mtp storage with invalid storage info.
  mtp_device_observer()->MtpStorageAttached(kStorageWithInvalidInfo);

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithInvalidInfo);

  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
}

}  // namespace storage_monitor
