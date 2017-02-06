// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_MAC_H_
#define DEVICE_HID_HID_SERVICE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <string>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/hid/hid_service.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class HidConnection;

class HidServiceMac : public HidService {
 public:
  HidServiceMac(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~HidServiceMac() override;

  void Connect(const HidDeviceId& device_id,
               const ConnectCallback& connect) override;

 private:
  // IOService matching callbacks.
  static void FirstMatchCallback(void* context, io_iterator_t iterator);
  static void TerminatedCallback(void* context, io_iterator_t iterator);

  void AddDevices();
  void RemoveDevices();

  static scoped_refptr<device::HidDeviceInfo> CreateDeviceInfo(
      io_service_t device);

  // Platform notification port.
  IONotificationPortRef notify_port_;
  base::mac::ScopedIOObject<io_iterator_t> devices_added_iterator_;
  base::mac::ScopedIOObject<io_iterator_t> devices_removed_iterator_;

  // The task runner for the thread on which this service was created.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The task runner for the FILE thread of the application using this service
  // on which slow running I/O operations can be performed.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_MAC_H_
