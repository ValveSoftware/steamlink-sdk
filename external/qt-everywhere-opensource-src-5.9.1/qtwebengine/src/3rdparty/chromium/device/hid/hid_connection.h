// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_H_
#define DEVICE_HID_HID_CONNECTION_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

namespace device {

class HidConnection : public base::RefCountedThreadSafe<HidConnection> {
 public:
  enum SpecialReportIds {
    kNullReportId = 0x00,
    kAnyReportId = 0xFF,
  };

  typedef base::Callback<
      void(bool success, scoped_refptr<net::IOBuffer> buffer, size_t size)>
      ReadCallback;
  typedef base::Callback<void(bool success)> WriteCallback;

  scoped_refptr<HidDeviceInfo> device_info() const { return device_info_; }
  bool has_protected_collection() const { return has_protected_collection_; }
  const base::ThreadChecker& thread_checker() const { return thread_checker_; }
  bool closed() const { return closed_; }

  // Closes the connection. This must be called before the object is freed.
  void Close();

  // The report ID (or 0 if report IDs are not supported by the device) is
  // always returned in the first byte of the buffer.
  void Read(const ReadCallback& callback);

  // The report ID (or 0 if report IDs are not supported by the device) is
  // always expected in the first byte of the buffer.
  void Write(scoped_refptr<net::IOBuffer> buffer,
             size_t size,
             const WriteCallback& callback);

  // The buffer will contain whatever report data was received from the device.
  // This may include the report ID. The report ID is not stripped because a
  // device may respond with other data in place of the report ID.
  void GetFeatureReport(uint8_t report_id, const ReadCallback& callback);

  // The report ID (or 0 if report IDs are not supported by the device) is
  // always expected in the first byte of the buffer.
  void SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                         size_t size,
                         const WriteCallback& callback);

 protected:
  friend class base::RefCountedThreadSafe<HidConnection>;

  explicit HidConnection(scoped_refptr<HidDeviceInfo> device_info);
  virtual ~HidConnection();

  virtual void PlatformClose() = 0;
  virtual void PlatformRead(const ReadCallback& callback) = 0;
  virtual void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                             size_t size,
                             const WriteCallback& callback) = 0;
  virtual void PlatformGetFeatureReport(uint8_t report_id,
                                        const ReadCallback& callback) = 0;
  virtual void PlatformSendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                         size_t size,
                                         const WriteCallback& callback) = 0;

  // PlatformRead implementation must call this method on read
  // success, rather than directly running the callback.
  // In case incoming buffer is empty or protected, it is filtered
  // and this method returns false. Otherwise it runs the callback
  // and returns true.
  bool CompleteRead(scoped_refptr<net::IOBuffer> buffer,
                    size_t size,
                    const ReadCallback& callback);

 private:
  bool IsReportIdProtected(uint8_t report_id);

  scoped_refptr<HidDeviceInfo> device_info_;
  bool has_protected_collection_;
  base::ThreadChecker thread_checker_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(HidConnection);
};

struct PendingHidReport {
  PendingHidReport();
  PendingHidReport(const PendingHidReport& other);
  ~PendingHidReport();

  scoped_refptr<net::IOBuffer> buffer;
  size_t size;
};

struct PendingHidRead {
  PendingHidRead();
  PendingHidRead(const PendingHidRead& other);
  ~PendingHidRead();

  HidConnection::ReadCallback callback;
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
