// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the data fetcher that GamepadProvider will use for android port.
// (GamepadPlatformDataFetcher).

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

class GamepadPlatformDataFetcherAndroid : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherAndroid();
  ~GamepadPlatformDataFetcherAndroid() override;

  void PauseHint(bool paused) override;

  void GetGamepadData(blink::WebGamepads* pads,
                      bool devices_changed_hint) override;

  // Registers the JNI methods for GamepadsReader.
  static bool RegisterGamepadPlatformDataFetcherAndroid(JNIEnv* env);

 private:
  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherAndroid);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
