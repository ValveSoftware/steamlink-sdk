// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "device/gamepad/gamepad_data_fetcher.h"

extern "C" {
struct udev_device;
}

namespace device {
class UdevLinux;
}

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadPlatformDataFetcherLinux
    : public GamepadDataFetcher {
 public:
  typedef GamepadDataFetcherFactoryImpl<GamepadPlatformDataFetcherLinux,
                                        GAMEPAD_SOURCE_LINUX_UDEV>
      Factory;

  GamepadPlatformDataFetcherLinux();
  ~GamepadPlatformDataFetcherLinux() override;

  GamepadSource source() override;

  // GamepadDataFetcher implementation.
  void GetGamepadData(bool devices_changed_hint) override;

 private:
  void OnAddedToProvider() override;

  void RefreshDevice(udev_device* dev);
  void EnumerateDevices();
  void ReadDeviceData(size_t index);

  // File descriptor for the /dev/input/js* devices. -1 if not in use.
  int device_fd_[blink::WebGamepads::itemsLengthCap];

  std::unique_ptr<device::UdevLinux> udev_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherLinux);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
