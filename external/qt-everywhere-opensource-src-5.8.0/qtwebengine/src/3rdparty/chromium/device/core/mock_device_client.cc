// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/core/mock_device_client.h"

#include "device/hid/mock_hid_service.h"
#include "device/usb/mock_usb_service.h"

namespace device {

MockDeviceClient::MockDeviceClient() {}

MockDeviceClient::~MockDeviceClient() {}

HidService* MockDeviceClient::GetHidService() {
  return hid_service();
}

UsbService* MockDeviceClient::GetUsbService() {
  return usb_service();
}

MockHidService* MockDeviceClient::hid_service() {
  if (!hid_service_)
    hid_service_.reset(new MockHidService());
  return hid_service_.get();
}

MockUsbService* MockDeviceClient::usb_service() {
  if (!usb_service_)
    usb_service_.reset(new MockUsbService());
  return usb_service_.get();
}

}  // namespace device
