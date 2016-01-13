// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_WIN_H_
#define DEVICE_HID_HID_CONNECTION_WIN_H_

#include <windows.h>

#include <set>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"

namespace device {

struct PendingHidTransfer;

class HidConnectionWin : public HidConnection {
 public:
  explicit HidConnectionWin(const HidDeviceInfo& device_info);

  bool available() const;

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

  void OnTransferFinished(scoped_refptr<PendingHidTransfer> transfer);
  void OnTransferCanceled(scoped_refptr<PendingHidTransfer> transfer);

 private:
  ~HidConnectionWin();

  base::win::ScopedHandle file_;
  std::set<scoped_refptr<PendingHidTransfer> > transfers_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionWin);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_WIN_H_
