// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_WIN_H_

#include <string>

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothServiceRecordWin {
 public:
  BluetoothServiceRecordWin(const std::string& name,
                            const std::string& address,
                            uint64 blob_size,
                            uint8* blob_data);

  BTH_ADDR bth_addr() const { return bth_addr_; }

  // The human-readable name of this service.
  const std::string& name() const { return name_; }

  // The address of the BluetoothDevice providing this service.
  const std::string& address() const { return address_; }

  // The UUID of the service.  This field may be empty if no UUID was
  // specified in the service record.
  const BluetoothUUID& uuid() const { return uuid_; }

  // Indicates if this service supports RFCOMM communication.
  bool SupportsRfcomm() const { return supports_rfcomm_; }

  // The RFCOMM channel to use, if this service supports RFCOMM communication.
  // The return value is undefined if SupportsRfcomm() returns false.
  uint8 rfcomm_channel() const { return rfcomm_channel_; }

 private:
  BTH_ADDR bth_addr_;
  std::string address_;
  std::string name_;
  BluetoothUUID uuid_;

  bool supports_rfcomm_;
  uint8 rfcomm_channel_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecordWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_WIN_H_
