// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_IMPL_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/webusb_descriptors.h"

struct libusb_device;
struct libusb_device_descriptor;
struct libusb_device_handle;
struct libusb_config_descriptor;

namespace base {
class SequencedTaskRunner;
}

namespace dbus {
class FileDescriptor;
}

namespace device {

class UsbDeviceHandleImpl;
class UsbContext;

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_config_descriptor* PlatformUsbConfigDescriptor;
typedef struct libusb_device_handle* PlatformUsbDeviceHandle;

class UsbDeviceImpl : public UsbDevice {
 public:
  // UsbDevice implementation:
  void Open(const OpenCallback& callback) override;

  // These functions are used during enumeration only. The values must not
  // change during the object's lifetime.
  void set_manufacturer_string(const base::string16& value) {
    manufacturer_string_ = value;
  }
  void set_product_string(const base::string16& value) {
    product_string_ = value;
  }
  void set_serial_number(const base::string16& value) {
    serial_number_ = value;
  }
  void set_webusb_allowed_origins(
      std::unique_ptr<WebUsbAllowedOrigins> allowed_origins) {
    webusb_allowed_origins_ = std::move(allowed_origins);
  }
  void set_webusb_landing_page(const GURL& url) { webusb_landing_page_ = url; }

  PlatformUsbDevice platform_device() const { return platform_device_; }

 protected:
  friend class UsbServiceImpl;
  friend class UsbDeviceHandleImpl;

  // Called by UsbServiceImpl only;
  UsbDeviceImpl(scoped_refptr<UsbContext> context,
                PlatformUsbDevice platform_device,
                const libusb_device_descriptor& descriptor,
                scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceImpl() override;

  void ReadAllConfigurations();
  void RefreshActiveConfiguration();

  // Called only by UsbServiceImpl.
  void set_visited(bool visited) { visited_ = visited; }
  bool was_visited() const { return visited_; }

 private:
  void GetAllConfigurations();
  void OpenOnBlockingThread(const OpenCallback& callback);
  void Opened(PlatformUsbDeviceHandle platform_handle,
              const OpenCallback& callback);

  base::ThreadChecker thread_checker_;
  PlatformUsbDevice platform_device_;
  bool visited_ = false;

  // Retain the context so that it will not be released before UsbDevice.
  scoped_refptr<UsbContext> context_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceImpl);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_IMPL_H_
