// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_MAC_H_
#define DEVICE_HID_HID_CONNECTION_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "device/hid/hid_connection.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class IOBuffer;
}

namespace device {

class HidConnectionMac : public HidConnection {
 public:
  HidConnectionMac(
      IOHIDDeviceRef device,
      scoped_refptr<HidDeviceInfo> device_info,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

 private:
  ~HidConnectionMac() override;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformRead(const ReadCallback& callback) override;
  void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const WriteCallback& callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                const ReadCallback& callback) override;
  void PlatformSendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const WriteCallback& callback) override;

  static void InputReportCallback(void* context,
                                  IOReturn result,
                                  void* sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t* report_bytes,
                                  CFIndex report_length);
  void ProcessInputReport(scoped_refptr<net::IOBufferWithSize> buffer);
  void ProcessReadQueue();
  void GetFeatureReportAsync(uint8_t report_id, const ReadCallback& callback);
  void SetReportAsync(IOHIDReportType report_type,
                      scoped_refptr<net::IOBuffer> buffer,
                      size_t size,
                      const WriteCallback& callback);
  void ReturnAsyncResult(const base::Closure& callback);

  base::ScopedCFTypeRef<IOHIDDeviceRef> device_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  std::vector<uint8_t> inbound_buffer_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_MAC_H_
