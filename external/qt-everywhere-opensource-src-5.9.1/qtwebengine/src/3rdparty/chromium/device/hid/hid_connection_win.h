// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_WIN_H_
#define DEVICE_HID_HID_CONNECTION_WIN_H_

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include <list>

#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "device/hid/hid_connection.h"

namespace device {

class PendingHidTransfer;

class HidConnectionWin : public HidConnection {
 public:
  HidConnectionWin(scoped_refptr<HidDeviceInfo> device_info,
                   base::win::ScopedHandle file);

 private:
  friend class HidServiceWin;
  friend class PendingHidTransfer;

  ~HidConnectionWin() override;

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

  void OnReadComplete(scoped_refptr<net::IOBuffer> buffer,
                      const ReadCallback& callback,
                      PendingHidTransfer* transfer,
                      bool signaled);
  void OnReadFeatureComplete(scoped_refptr<net::IOBuffer> buffer,
                             const ReadCallback& callback,
                             PendingHidTransfer* transfer,
                             bool signaled);
  void OnWriteComplete(const WriteCallback& callback,
                       PendingHidTransfer* transfer,
                       bool signaled);

  std::unique_ptr<PendingHidTransfer> UnlinkTransfer(
      PendingHidTransfer* transfer);

  base::win::ScopedHandle file_;

  std::list<std::unique_ptr<PendingHidTransfer>> transfers_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionWin);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_WIN_H_
