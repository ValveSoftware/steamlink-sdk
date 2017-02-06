// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the default data fetcher that GamepadProvider will use if none is
// supplied. (GamepadPlatformDataFetcher).

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"

#if defined(OS_ANDROID)
#include "device/gamepad/gamepad_platform_data_fetcher_android.h"
#elif defined(OS_WIN)
#include "device/gamepad/gamepad_platform_data_fetcher_win.h"
#elif defined(OS_MACOSX)
#include "device/gamepad/gamepad_platform_data_fetcher_mac.h"
#elif defined(OS_LINUX)
#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"
#endif

namespace device {

#if defined(OS_ANDROID)

typedef GamepadPlatformDataFetcherAndroid GamepadPlatformDataFetcher;

#elif defined(OS_WIN)

typedef GamepadPlatformDataFetcherWin GamepadPlatformDataFetcher;

#elif defined(OS_MACOSX)

typedef GamepadPlatformDataFetcherMac GamepadPlatformDataFetcher;

#elif defined(OS_LINUX) && defined(USE_UDEV)

typedef GamepadPlatformDataFetcherLinux GamepadPlatformDataFetcher;

#else

class GamepadDataFetcherEmpty : public GamepadDataFetcher {
 public:
  GamepadDataFetcherEmpty();

  void GetGamepadData(blink::WebGamepads* pads,
                      bool devices_changed_hint) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GamepadDataFetcherEmpty);
};
typedef GamepadDataFetcherEmpty GamepadPlatformDataFetcher;

#endif

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
