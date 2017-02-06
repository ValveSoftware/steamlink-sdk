// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webusb/webusb_detector.h"

#include "components/webusb/webusb_browser_client.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_ids.h"

namespace webusb {

WebUsbDetector::WebUsbDetector(WebUsbBrowserClient* webusb_browser_client)
    : webusb_browser_client_(webusb_browser_client), observer_(this) {
  Initialize();
}

WebUsbDetector::~WebUsbDetector() {}

void WebUsbDetector::Initialize() {
  if (!webusb_browser_client_) {
    return;
  }

  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  if (!usb_service)
    return;

  observer_.Add(usb_service);
}

void WebUsbDetector::OnDeviceAdded(scoped_refptr<device::UsbDevice> device) {
  const base::string16& product_name = device->product_string();
  if (product_name.empty()) {
    return;
  }

  const GURL& landing_page = device->webusb_landing_page();
  if (!landing_page.is_valid()) {
    return;
  }

  std::string notification_id = device->guid();

  webusb_browser_client_->OnDeviceAdded(product_name, landing_page,
                                        notification_id);
}

void WebUsbDetector::OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) {
  std::string notification_id = device->guid();
  webusb_browser_client_->OnDeviceRemoved(notification_id);
}

}  // namespace webusb
