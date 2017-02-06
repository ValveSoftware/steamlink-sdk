// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/core/device_info_query_win.h"

#include <stddef.h>
#include <string.h>

#include "base/strings/string_util.h"

namespace device {

DeviceInfoQueryWin::DeviceInfoQueryWin()
    : device_info_list_(SetupDiCreateDeviceInfoList(nullptr, nullptr)) {
  memset(&device_info_data_, 0, sizeof(device_info_data_));
}

DeviceInfoQueryWin::~DeviceInfoQueryWin() {
  if (device_info_list_valid()) {
    // Release |device_info_data_| only when it is valid.
    if (device_info_data_.cbSize != 0)
      SetupDiDeleteDeviceInfo(device_info_list_, &device_info_data_);
    SetupDiDestroyDeviceInfoList(device_info_list_);
  }
}

bool DeviceInfoQueryWin::AddDevice(const char* device_path) {
  return SetupDiOpenDeviceInterfaceA(device_info_list_, device_path, 0,
                                     nullptr) != FALSE;
}

bool DeviceInfoQueryWin::GetDeviceInfo() {
  DCHECK_EQ(0U, device_info_data_.cbSize);
  device_info_data_.cbSize = sizeof(device_info_data_);
  if (!SetupDiEnumDeviceInfo(device_info_list_, 0, &device_info_data_)) {
    // Clear cbSize to maintain the invariant.
    device_info_data_.cbSize = 0;
    return false;
  }
  return true;
}

bool DeviceInfoQueryWin::GetDeviceStringProperty(DWORD property,
                                                 std::string* property_buffer) {
  DWORD property_reg_data_type;
  const size_t property_buffer_length = 512;
  if (!SetupDiGetDeviceRegistryPropertyA(
          device_info_list_, &device_info_data_, property,
          &property_reg_data_type,
          reinterpret_cast<PBYTE>(
              base::WriteInto(property_buffer, property_buffer_length)),
          static_cast<DWORD>(property_buffer_length), nullptr))
    return false;

  if (property_reg_data_type != REG_SZ)
    return false;

  // Shrink |property_buffer| down to its correct size.
  size_t eos = property_buffer->find('\0');
  if (eos != std::string::npos)
    property_buffer->resize(eos);

  return true;
}

}  // namespace device
