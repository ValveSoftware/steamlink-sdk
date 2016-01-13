// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_platform_data_fetcher_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

#include "jni/GamepadList_jni.h"

#include "third_party/WebKit/public/platform/WebGamepads.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;
using blink::WebGamepad;
using blink::WebGamepads;

namespace content {

bool
GamepadPlatformDataFetcherAndroid::RegisterGamepadPlatformDataFetcherAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

GamepadPlatformDataFetcherAndroid::GamepadPlatformDataFetcherAndroid() {
  PauseHint(false);
}

GamepadPlatformDataFetcherAndroid::~GamepadPlatformDataFetcherAndroid() {
  PauseHint(true);
}

void GamepadPlatformDataFetcherAndroid::GetGamepadData(
    blink::WebGamepads* pads,
    bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  pads->length = 0;

  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;

  Java_GamepadList_updateGamepadData(env, reinterpret_cast<intptr_t>(pads));
}

void GamepadPlatformDataFetcherAndroid::PauseHint(bool paused) {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;

  Java_GamepadList_notifyForGamepadsAccess(env, paused);
}

static void SetGamepadData(JNIEnv* env,
                           jobject obj,
                           jlong gamepads,
                           jint index,
                           jboolean mapping,
                           jboolean connected,
                           jstring devicename,
                           jlong timestamp,
                           jfloatArray jaxes,
                           jfloatArray jbuttons) {
  DCHECK(gamepads);
  blink::WebGamepads* pads = reinterpret_cast<WebGamepads*>(gamepads);
  DCHECK_EQ(pads->length, unsigned(index));
  DCHECK_LT(index, static_cast<int>(blink::WebGamepads::itemsLengthCap));

  ++pads->length;

  blink::WebGamepad& pad = pads->items[index];

  pad.connected = connected;

  pad.timestamp = timestamp;

  // Do not set gamepad parameters for all the gamepad devices that are not
  // attached.
  if (!connected)
    return;

  // Map the Gamepad DeviceName String to the WebGamepad Id. Ideally it should
  // be mapped to vendor and product information but it is only available at
  // kernel level and it can not be queried using class
  // android.hardware.input.InputManager.
  // TODO(SaurabhK): Store a cached WebGamePad object in
  // GamepadPlatformDataFetcherAndroid and only update constant WebGamepad
  // values when a device has changed.
  base::string16 device_name;
  base::android::ConvertJavaStringToUTF16(env, devicename, &device_name);
  const size_t name_to_copy =
      std::min(device_name.size(), WebGamepad::idLengthCap - 1);
  memcpy(pad.id,
         device_name.data(),
         name_to_copy * sizeof(base::string16::value_type));
  pad.id[name_to_copy] = 0;

  base::string16 mapping_name = base::UTF8ToUTF16(mapping ? "standard" : "");
  const size_t mapping_to_copy =
      std::min(mapping_name.size(), WebGamepad::mappingLengthCap - 1);
  memcpy(pad.mapping,
         mapping_name.data(),
         mapping_to_copy * sizeof(base::string16::value_type));
  pad.mapping[mapping_to_copy] = 0;

  pad.timestamp = timestamp;

  std::vector<float> axes;
  base::android::JavaFloatArrayToFloatVector(env, jaxes, &axes);

  // Set WebGamepad axeslength to total number of axes on the gamepad device.
  // Only return the first axesLengthCap if axeslength captured by GamepadList
  // is larger than axesLengthCap.
  pad.axesLength = std::min(static_cast<int>(axes.size()),
                            static_cast<int>(WebGamepad::axesLengthCap));

  // Copy axes state to the WebGamepad axes[].
  for (unsigned int i = 0; i < pad.axesLength; i++) {
    pad.axes[i] = static_cast<double>(axes[i]);
  }

  std::vector<float> buttons;
  base::android::JavaFloatArrayToFloatVector(env, jbuttons, &buttons);

  // Set WebGamepad buttonslength to total number of axes on the gamepad
  // device. Only return the first buttonsLengthCap if axeslength captured by
  // GamepadList is larger than buttonsLengthCap.
  pad.buttonsLength = std::min(static_cast<int>(buttons.size()),
                               static_cast<int>(WebGamepad::buttonsLengthCap));

  // Copy buttons state to the WebGamepad buttons[].
  for (unsigned int j = 0; j < pad.buttonsLength; j++) {
    pad.buttons[j].pressed = buttons[j];
    pad.buttons[j].value = buttons[j];
  }
}

}  // namespace content
