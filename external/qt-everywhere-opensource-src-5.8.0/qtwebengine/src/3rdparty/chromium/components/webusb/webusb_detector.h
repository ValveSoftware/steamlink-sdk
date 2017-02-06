// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBUSB_WEBUSB_DETECTOR_H_
#define COMPONENTS_WEBUSB_WEBUSB_DETECTOR_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "device/usb/usb_service.h"

namespace device {
class UsbDevice;
}

namespace webusb {

class UsbDevice;
class WebUsbBrowserClient;

class WebUsbDetector : public device::UsbService::Observer {
 public:
  explicit WebUsbDetector(WebUsbBrowserClient* webusb_browser_client);

  ~WebUsbDetector() override;

 private:
  // Initializes the WebUsbDetector.
  void Initialize();

  // UsbService::observer override:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;

  // UsbService::observer override:
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  WebUsbBrowserClient* webusb_browser_client_ = nullptr;

  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbDetector);
};

}  // namespace webusb

#endif  // COMPONENTS_WEBUSB_WEBUSB_DETECTOR_H_
