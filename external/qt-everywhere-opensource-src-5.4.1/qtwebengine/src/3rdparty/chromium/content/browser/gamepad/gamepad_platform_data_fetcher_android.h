// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the data fetcher that GamepadProvider will use for android port.
// (GamepadPlatformDataFetcher).

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

class GamepadPlatformDataFetcherAndroid : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherAndroid();
  virtual ~GamepadPlatformDataFetcherAndroid();

  virtual void PauseHint(bool paused) OVERRIDE;

  virtual void GetGamepadData(blink::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;

  // Registers the JNI methods for GamepadsReader.
  static bool RegisterGamepadPlatformDataFetcherAndroid(JNIEnv* env);

 private:
  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
