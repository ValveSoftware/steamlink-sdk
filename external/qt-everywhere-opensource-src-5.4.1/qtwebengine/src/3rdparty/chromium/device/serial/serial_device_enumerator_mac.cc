// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_device_enumerator_mac.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"

namespace device {

// static
scoped_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {
  return scoped_ptr<SerialDeviceEnumerator>(new SerialDeviceEnumeratorMac());
}

SerialDeviceEnumeratorMac::SerialDeviceEnumeratorMac() {}

SerialDeviceEnumeratorMac::~SerialDeviceEnumeratorMac() {}

// TODO(rockot): Use IOKit to enumerate serial interfaces.
mojo::Array<SerialDeviceInfoPtr> SerialDeviceEnumeratorMac::GetDevices() {
  const base::FilePath kDevRoot("/dev");
  const int kFilesAndSymLinks =
      base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS;

  std::set<std::string> valid_patterns;
  valid_patterns.insert("/dev/*Bluetooth*");
  valid_patterns.insert("/dev/*Modem*");
  valid_patterns.insert("/dev/*bluetooth*");
  valid_patterns.insert("/dev/*modem*");
  valid_patterns.insert("/dev/*serial*");
  valid_patterns.insert("/dev/tty.*");
  valid_patterns.insert("/dev/cu.*");

  mojo::Array<SerialDeviceInfoPtr> devices;
  base::FileEnumerator enumerator(kDevRoot, false, kFilesAndSymLinks);
  do {
    const base::FilePath next_device_path(enumerator.Next());
    const std::string next_device = next_device_path.value();
    if (next_device.empty())
      break;

    std::set<std::string>::const_iterator i = valid_patterns.begin();
    for (; i != valid_patterns.end(); ++i) {
      if (MatchPattern(next_device, *i)) {
        SerialDeviceInfoPtr info(SerialDeviceInfo::New());
        info->path = next_device;
        devices.push_back(info.Pass());
        break;
      }
    }
  } while (true);
  return devices.Pass();
}

}  // namespace device
