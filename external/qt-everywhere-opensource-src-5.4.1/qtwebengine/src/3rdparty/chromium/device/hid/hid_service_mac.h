// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_MAC_H_
#define DEVICE_HID_HID_SERVICE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#include <string>

#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "device/hid/hid_service.h"

namespace base {
class MessageLoopProxy;
}

namespace device {

class HidConnection;

class HidServiceMac : public HidService {
 public:
  HidServiceMac();

  virtual scoped_refptr<HidConnection> Connect(const HidDeviceId& device_id)
      OVERRIDE;

 private:
  virtual ~HidServiceMac();

  void StartWatchingDevices();
  void StopWatchingDevices();

  // Device changing callbacks.
  static void AddDeviceCallback(void* context,
                                IOReturn result,
                                void* sender,
                                IOHIDDeviceRef hid_device);
  static void RemoveDeviceCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDDeviceRef hid_device);

  void Enumerate();

  void PlatformAddDevice(IOHIDDeviceRef hid_device);
  void PlatformRemoveDevice(IOHIDDeviceRef hid_device);

  // Platform HID Manager
  base::ScopedCFTypeRef<IOHIDManagerRef> hid_manager_;

  // The message loop for the thread on which this service was created.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_MAC_H_
