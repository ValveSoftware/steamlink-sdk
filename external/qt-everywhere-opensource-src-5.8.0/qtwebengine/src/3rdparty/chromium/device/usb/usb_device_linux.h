// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_IMPL_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "device/usb/usb_device.h"
#include "device/usb/webusb_descriptors.h"

namespace base {
class SequencedTaskRunner;
}

namespace dbus {
class FileDescriptor;
}

namespace device {

struct UsbConfigDescriptor;
struct UsbDeviceDescriptor;

class UsbDeviceLinux : public UsbDevice {
 public:
// UsbDevice implementation:
#if defined(OS_CHROMEOS)
  void CheckUsbAccess(const ResultCallback& callback) override;
#endif  // OS_CHROMEOS
  void Open(const OpenCallback& callback) override;

  const std::string& device_path() const { return device_path_; }

  // These functions are used during enumeration only. The values must not
  // change during the object's lifetime.
  void set_webusb_allowed_origins(
      std::unique_ptr<WebUsbAllowedOrigins> allowed_origins) {
    webusb_allowed_origins_ = std::move(allowed_origins);
  }
  void set_webusb_landing_page(const GURL& url) { webusb_landing_page_ = url; }

 protected:
  friend class UsbServiceLinux;

  // Called by UsbServiceLinux only.
  UsbDeviceLinux(const std::string& device_path,
                 const UsbDeviceDescriptor& descriptor,
                 const std::string& manufacturer_string,
                 const std::string& product_string,
                 const std::string& serial_number,
                 uint8_t active_configuration,
                 scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceLinux() override;

 private:
#if defined(OS_CHROMEOS)
  void OnOpenRequestComplete(const OpenCallback& callback,
                             dbus::FileDescriptor fd);
  void OnOpenRequestError(const OpenCallback& callback,
                          const std::string& error_name,
                          const std::string& error_message);
  void OpenOnBlockingThreadWithFd(dbus::FileDescriptor fd,
                                  const OpenCallback& callback);
#else
  void OpenOnBlockingThread(const OpenCallback& callback);
#endif  // defined(OS_CHROMEOS)
  void Opened(base::ScopedFD fd, const OpenCallback& callback);

  base::ThreadChecker thread_checker_;

  const std::string device_path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceLinux);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_IMPL_H_
