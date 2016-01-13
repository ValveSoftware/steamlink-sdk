// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_H_
#define DEVICE_HID_HID_CONNECTION_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

namespace device {

class HidConnection : public base::RefCountedThreadSafe<HidConnection> {
 public:
  typedef base::Callback<void(bool success, size_t size)> IOCallback;

  virtual void Read(scoped_refptr<net::IOBufferWithSize> buffer,
                    const IOCallback& callback) = 0;
  virtual void Write(uint8_t report_id,
                     scoped_refptr<net::IOBufferWithSize> buffer,
                     const IOCallback& callback) = 0;
  virtual void GetFeatureReport(uint8_t report_id,
                                scoped_refptr<net::IOBufferWithSize> buffer,
                                const IOCallback& callback) = 0;
  virtual void SendFeatureReport(uint8_t report_id,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 const IOCallback& callback) = 0;

  const HidDeviceInfo& device_info() const { return device_info_; }

 protected:
  friend class base::RefCountedThreadSafe<HidConnection>;
  friend struct HidDeviceInfo;

  explicit HidConnection(const HidDeviceInfo& device_info);
  virtual ~HidConnection();

 private:
  const HidDeviceInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(HidConnection);
};

struct PendingHidReport {
  PendingHidReport();
  ~PendingHidReport();

  scoped_refptr<net::IOBufferWithSize> buffer;
};

struct PendingHidRead {
  PendingHidRead();
  ~PendingHidRead();

  scoped_refptr<net::IOBufferWithSize> buffer;
  HidConnection::IOCallback callback;
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
