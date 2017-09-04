// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_LINUX_H_
#define DEVICE_HID_HID_CONNECTION_LINUX_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/hid/hid_connection.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class HidConnectionLinux : public HidConnection {
 public:
  HidConnectionLinux(
      scoped_refptr<HidDeviceInfo> device_info,
      base::File device_file,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_runner);

 private:
  friend class base::RefCountedThreadSafe<HidConnectionLinux>;
  class FileThreadHelper;

  typedef base::Callback<void(ssize_t)> InternalWriteCallback;
  typedef base::Callback<void(int)> IoctlCallback;

  ~HidConnectionLinux() override;

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

  // Callbacks for blocking operations run on the FILE thread.
  void FinishWrite(size_t expected_size,
                   const WriteCallback& callback,
                   ssize_t result);
  void FinishGetFeatureReport(uint8_t report_id,
                              scoped_refptr<net::IOBuffer> buffer,
                              const ReadCallback& callback,
                              int result);
  void FinishSendFeatureReport(const WriteCallback& callback, int result);

  // Writes to the device. This operation may block.
  static void BlockingWrite(
      base::PlatformFile platform_file,
      scoped_refptr<net::IOBuffer> buffer,
      size_t size,
      const InternalWriteCallback& callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  // Performs an ioctl on the device. This operation may block.
  static void BlockingIoctl(
      base::PlatformFile platform_file,
      int request,
      scoped_refptr<net::IOBuffer> buffer,
      const IoctlCallback& callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Closes the device file descriptor. Must be called on the FILE thread.
  static void CloseDevice(base::File device_file);

  void ProcessInputReport(scoped_refptr<net::IOBuffer> buffer, size_t size);
  void ProcessReadQueue();

  base::File device_file_;
  FileThreadHelper* helper_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  std::queue<PendingHidReport> pending_reports_;
  std::queue<PendingHidRead> pending_reads_;

  base::WeakPtrFactory<HidConnectionLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_LINUX_H_
