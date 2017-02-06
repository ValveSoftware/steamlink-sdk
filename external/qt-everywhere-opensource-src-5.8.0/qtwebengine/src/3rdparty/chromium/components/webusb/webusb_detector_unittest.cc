// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webusb/webusb_detector.h"

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/webusb/webusb_browser_client.h"
#include "device/core/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// usb device product name
const char* kProductName_1 = "Google Product A";
const char* kProductName_2 = "Google Product B";
const char* kProductName_3 = "Google Product C";

// usb device landing page
const char* kLandingPage_1 = "https://www.google.com/A";
const char* kLandingPage_2 = "https://www.google.com/B";
const char* kLandingPage_3 = "https://www.google.com/C";

}  // namespace

namespace webusb {

namespace {

class MockWebUsbBrowserClient : public webusb::WebUsbBrowserClient {
 public:
  MockWebUsbBrowserClient() {}

  ~MockWebUsbBrowserClient() override {}

  // webusb::WebUsbBrowserClient implementation
  MOCK_METHOD3(OnDeviceAdded,
               void(const base::string16& product_name,
                    const GURL& landing_page,
                    const std::string& notification_id));

  // webusb::WebUsbBrowserClient implementation
  MOCK_METHOD1(OnDeviceRemoved, void(const std::string& notification_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebUsbBrowserClient);
};

}  // namespace

class WebUsbDetectorTest : public testing::Test {
 public:
  WebUsbDetectorTest() {}

  ~WebUsbDetectorTest() override = default;

 protected:
  device::MockDeviceClient device_client_;
  MockWebUsbBrowserClient mock_webusb_browser_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUsbDetectorTest);
};

TEST_F(WebUsbDetectorTest, UsbDeviceAdded) {
  base::string16 product_name = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name, landing_page, guid))
      .Times(1);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(testing::_))
      .Times(0);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->AddDevice(device);
}

TEST_F(WebUsbDetectorTest, UsbDeviceAddedAndRemoved) {
  base::string16 product_name = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  {
    testing::InSequence s;
    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name, landing_page, guid))
        .Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid)).Times(1);
  }

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->AddDevice(device);
  device_client_.usb_service()->RemoveDevice(device);
}

TEST_F(WebUsbDetectorTest, UsbDeviceWithoutProductNameAddedAndRemoved) {
  std::string product_name = "";
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", product_name, "002", landing_page));
  std::string guid = device->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid)).Times(1);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->AddDevice(device);
  device_client_.usb_service()->RemoveDevice(device);
}

TEST_F(WebUsbDetectorTest, UsbDeviceWithoutLandingPageAddedAndRemoved) {
  GURL landing_page("");
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid)).Times(1);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->AddDevice(device);
  device_client_.usb_service()->RemoveDevice(device);
}

TEST_F(WebUsbDetectorTest, WebUsbBrowserClientIsNullptr) {
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(testing::_))
      .Times(0);

  webusb::WebUsbDetector webusb_detector(nullptr);

  device_client_.usb_service()->AddDevice(device);
  device_client_.usb_service()->RemoveDevice(device);
}

TEST_F(WebUsbDetectorTest, NoUsbService) {
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(testing::_))
      .Times(0);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);
}

TEST_F(WebUsbDetectorTest, UsbDeviceWasThereBeforeAndThenRemoved) {
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid)).Times(1);

  // usb device was added before webusb_detector was created
  device_client_.usb_service()->AddDevice(device);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->RemoveDevice(device);
}

TEST_F(
    WebUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedBeforeWebUsbDetectorWasCreated) {
  base::string16 product_name_1 = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page_1(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device_1(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page_1));
  std::string guid_1 = device_1->guid();

  base::string16 product_name_2 = base::UTF8ToUTF16(kProductName_2);
  GURL landing_page_2(kLandingPage_2);
  scoped_refptr<device::MockUsbDevice> device_2(new device::MockUsbDevice(
      3, 4, "Google", kProductName_2, "005", landing_page_2));
  std::string guid_2 = device_2->guid();

  base::string16 product_name_3 = base::UTF8ToUTF16(kProductName_3);
  GURL landing_page_3(kLandingPage_3);
  scoped_refptr<device::MockUsbDevice> device_3(new device::MockUsbDevice(
      6, 7, "Google", kProductName_3, "008", landing_page_3));
  std::string guid_3 = device_3->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_1, landing_page_1, guid_1))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_2, landing_page_2, guid_2))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_3, landing_page_3, guid_3))
      .Times(0);

  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_1)).Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_2)).Times(0);
  EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_3)).Times(0);

  // three usb devices were added and removed before webusb_detector was
  // created
  device_client_.usb_service()->AddDevice(device_1);
  device_client_.usb_service()->AddDevice(device_2);
  device_client_.usb_service()->AddDevice(device_3);

  device_client_.usb_service()->RemoveDevice(device_1);
  device_client_.usb_service()->RemoveDevice(device_2);
  device_client_.usb_service()->RemoveDevice(device_3);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);
}

TEST_F(
    WebUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedAfterWebUsbDetectorWasCreated) {
  base::string16 product_name_1 = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page_1(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device_1(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page_1));
  std::string guid_1 = device_1->guid();

  base::string16 product_name_2 = base::UTF8ToUTF16(kProductName_2);
  GURL landing_page_2(kLandingPage_2);
  scoped_refptr<device::MockUsbDevice> device_2(new device::MockUsbDevice(
      3, 4, "Google", kProductName_2, "005", landing_page_2));
  std::string guid_2 = device_2->guid();

  base::string16 product_name_3 = base::UTF8ToUTF16(kProductName_3);
  GURL landing_page_3(kLandingPage_3);
  scoped_refptr<device::MockUsbDevice> device_3(new device::MockUsbDevice(
      6, 7, "Google", kProductName_3, "008", landing_page_3));
  std::string guid_3 = device_3->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_1, landing_page_1, guid_1))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_2, landing_page_2, guid_2))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_3, landing_page_3, guid_3))
      .Times(0);

  {
    testing::InSequence s;

    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_1)).Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_2)).Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_3)).Times(1);
  }

  // three usb devices were added before webusb_detector was created
  device_client_.usb_service()->AddDevice(device_1);
  device_client_.usb_service()->AddDevice(device_2);
  device_client_.usb_service()->AddDevice(device_3);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->RemoveDevice(device_1);
  device_client_.usb_service()->RemoveDevice(device_2);
  device_client_.usb_service()->RemoveDevice(device_3);
}

TEST_F(WebUsbDetectorTest,
       TwoUsbDevicesWereThereBeforeAndThenRemovedAndNewUsbDeviceAdded) {
  base::string16 product_name_1 = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page_1(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device_1(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page_1));
  std::string guid_1 = device_1->guid();

  base::string16 product_name_2 = base::UTF8ToUTF16(kProductName_2);
  GURL landing_page_2(kLandingPage_2);
  scoped_refptr<device::MockUsbDevice> device_2(new device::MockUsbDevice(
      3, 4, "Google", kProductName_2, "005", landing_page_2));
  std::string guid_2 = device_2->guid();

  base::string16 product_name_3 = base::UTF8ToUTF16(kProductName_3);
  GURL landing_page_3(kLandingPage_3);
  scoped_refptr<device::MockUsbDevice> device_3(new device::MockUsbDevice(
      6, 7, "Google", kProductName_3, "008", landing_page_3));
  std::string guid_3 = device_3->guid();

  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_1, landing_page_1, guid_1))
      .Times(0);
  EXPECT_CALL(mock_webusb_browser_client_,
              OnDeviceAdded(product_name_3, landing_page_3, guid_3))
      .Times(0);

  {
    testing::InSequence s;

    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_1)).Times(1);
    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_2, landing_page_2, guid_2))
        .Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_3)).Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_2)).Times(1);
  }

  // two usb devices were added before webusb_detector was created
  device_client_.usb_service()->AddDevice(device_1);
  device_client_.usb_service()->AddDevice(device_3);

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->RemoveDevice(device_1);
  device_client_.usb_service()->AddDevice(device_2);
  device_client_.usb_service()->RemoveDevice(device_3);
  device_client_.usb_service()->RemoveDevice(device_2);
}

TEST_F(WebUsbDetectorTest, ThreeUsbDevicesAddedAndRemoved) {
  base::string16 product_name_1 = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page_1(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device_1(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page_1));
  std::string guid_1 = device_1->guid();

  base::string16 product_name_2 = base::UTF8ToUTF16(kProductName_2);
  GURL landing_page_2(kLandingPage_2);
  scoped_refptr<device::MockUsbDevice> device_2(new device::MockUsbDevice(
      3, 4, "Google", kProductName_2, "005", landing_page_2));
  std::string guid_2 = device_2->guid();

  base::string16 product_name_3 = base::UTF8ToUTF16(kProductName_3);
  GURL landing_page_3(kLandingPage_3);
  scoped_refptr<device::MockUsbDevice> device_3(new device::MockUsbDevice(
      6, 7, "Google", kProductName_3, "008", landing_page_3));
  std::string guid_3 = device_3->guid();

  {
    testing::InSequence s;

    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_1, landing_page_1, guid_1))
        .Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_1)).Times(1);

    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_2, landing_page_2, guid_2))
        .Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_2)).Times(1);

    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_3, landing_page_3, guid_3))
        .Times(1);
    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_3)).Times(1);
  }

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->AddDevice(device_1);
  device_client_.usb_service()->RemoveDevice(device_1);
  device_client_.usb_service()->AddDevice(device_2);
  device_client_.usb_service()->RemoveDevice(device_2);
  device_client_.usb_service()->AddDevice(device_3);
  device_client_.usb_service()->RemoveDevice(device_3);
}

TEST_F(WebUsbDetectorTest, ThreeUsbDeviceAddedAndRemovedDifferentOrder) {
  base::string16 product_name_1 = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page_1(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device_1(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page_1));
  std::string guid_1 = device_1->guid();

  base::string16 product_name_2 = base::UTF8ToUTF16(kProductName_2);
  GURL landing_page_2(kLandingPage_2);
  scoped_refptr<device::MockUsbDevice> device_2(new device::MockUsbDevice(
      3, 4, "Google", kProductName_2, "005", landing_page_2));
  std::string guid_2 = device_2->guid();

  base::string16 product_name_3 = base::UTF8ToUTF16(kProductName_3);
  GURL landing_page_3(kLandingPage_3);
  scoped_refptr<device::MockUsbDevice> device_3(new device::MockUsbDevice(
      6, 7, "Google", kProductName_3, "008", landing_page_3));
  std::string guid_3 = device_3->guid();

  {
    testing::InSequence s;

    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_1, landing_page_1, guid_1))
        .Times(1);

    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_2, landing_page_2, guid_2))
        .Times(1);

    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_2)).Times(1);

    EXPECT_CALL(mock_webusb_browser_client_,
                OnDeviceAdded(product_name_3, landing_page_3, guid_3))
        .Times(1);

    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_1)).Times(1);

    EXPECT_CALL(mock_webusb_browser_client_, OnDeviceRemoved(guid_3)).Times(1);
  }

  webusb::WebUsbDetector webusb_detector(&mock_webusb_browser_client_);

  device_client_.usb_service()->AddDevice(device_1);
  device_client_.usb_service()->AddDevice(device_2);
  device_client_.usb_service()->RemoveDevice(device_2);
  device_client_.usb_service()->AddDevice(device_3);
  device_client_.usb_service()->RemoveDevice(device_1);
  device_client_.usb_service()->RemoveDevice(device_3);
}

}  // namespace webusb
