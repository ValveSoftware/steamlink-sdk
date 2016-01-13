// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_MAC_H_
#define DEVICE_HID_HID_CONNECTION_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#include <queue>

#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class IOBuffer;
}

namespace device {

class HidConnectionMac : public HidConnection {
 public:
  explicit HidConnectionMac(HidDeviceInfo device_info);

  virtual void Read(scoped_refptr<net::IOBufferWithSize> buffer,
                    const IOCallback& callback) OVERRIDE;
  virtual void Write(uint8_t report_id,
                     scoped_refptr<net::IOBufferWithSize> buffer,
                     const IOCallback& callback) OVERRIDE;
  virtual void GetFeatureReport(uint8_t report_id,
                                scoped_refptr<net::IOBufferWithSize> buffer,
                                const IOCallback& callback) OVERRIDE;
  virtual void SendFeatureReport(uint8_t report_id,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 const IOCallback& callback) OVERRIDE;

 private:
  virtual ~HidConnectionMac();

  static void InputReportCallback(void* context,
                                  IOReturn result,
                                  void* sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t* report_bytes,
                                  CFIndex report_length);
  void ProcessReadQueue();
  void ProcessInputReport(IOHIDReportType type,
                          scoped_refptr<net::IOBufferWithSize> buffer);

  void WriteReport(IOHIDReportType type,
                   uint8_t report_id,
                   scoped_refptr<net::IOBufferWithSize> buffer,
                   const IOCallback& callback);

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  base::ScopedCFTypeRef<IOHIDDeviceRef> device_;
  scoped_ptr<uint8_t, base::FreeDeleter> inbound_buffer_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionMac);
};


}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_MAC_H_
