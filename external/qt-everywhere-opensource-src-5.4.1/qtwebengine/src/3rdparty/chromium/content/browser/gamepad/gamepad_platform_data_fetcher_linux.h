// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

extern "C" {
struct udev_device;
}

namespace content {

class UdevLinux;

class GamepadPlatformDataFetcherLinux : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherLinux();
  virtual ~GamepadPlatformDataFetcherLinux();

  // GamepadDataFetcher implementation.
  virtual void GetGamepadData(blink::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;

 private:
  void RefreshDevice(udev_device* dev);
  void EnumerateDevices();
  void ReadDeviceData(size_t index);

  // File descriptors for the /dev/input/js* devices. -1 if not in use.
  int device_fds_[blink::WebGamepads::itemsLengthCap];

  // Functions to map from device data to standard layout, if available. May
  // be null if no mapping is available.
  GamepadStandardMappingFunction mappers_[blink::WebGamepads::itemsLengthCap];

  // Data that's returned to the consumer.
  blink::WebGamepads data_;

  scoped_ptr<UdevLinux> udev_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
