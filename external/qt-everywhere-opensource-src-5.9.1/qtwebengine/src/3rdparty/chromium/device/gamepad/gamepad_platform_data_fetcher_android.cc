// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"

#include "jni/GamepadList_jni.h"

#include "third_party/WebKit/public/platform/WebGamepads.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using blink::WebGamepad;
using blink::WebGamepads;

namespace device {

bool GamepadPlatformDataFetcherAndroid::
    RegisterGamepadPlatformDataFetcherAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

GamepadPlatformDataFetcherAndroid::GamepadPlatformDataFetcherAndroid() {
}

GamepadPlatformDataFetcherAndroid::~GamepadPlatformDataFetcherAndroid() {
  PauseHint(true);
}

GamepadSource GamepadPlatformDataFetcherAndroid::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherAndroid::OnAddedToProvider() {
  PauseHint(false);
}

void GamepadPlatformDataFetcherAndroid::GetGamepadData(
    bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;

  Java_GamepadList_updateGamepadData(env, reinterpret_cast<intptr_t>(this));
}

void GamepadPlatformDataFetcherAndroid::PauseHint(bool paused) {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;

  Java_GamepadList_setGamepadAPIActive(env, !paused);
}

static void SetGamepadData(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jlong data_fetcher,
                           jint index,
                           jboolean mapping,
                           jboolean connected,
                           const JavaParamRef<jstring>& devicename,
                           jlong timestamp,
                           const JavaParamRef<jfloatArray>& jaxes,
                           const JavaParamRef<jfloatArray>& jbuttons) {
  DCHECK(data_fetcher);
  GamepadPlatformDataFetcherAndroid* fetcher =
      reinterpret_cast<GamepadPlatformDataFetcherAndroid*>(data_fetcher);
  DCHECK_LT(index, static_cast<int>(blink::WebGamepads::itemsLengthCap));

  // Do not set gamepad parameters for all the gamepad devices that are not
  // attached.
  if (!connected)
    return;

  PadState* state = fetcher->GetPadState(index);

  if (!state)
    return;

  blink::WebGamepad& pad = state->data;

  // Is this the first time we've seen this device?
  if (state->active_state == GAMEPAD_NEWLY_ACTIVE) {
    // Map the Gamepad DeviceName String to the WebGamepad Id. Ideally it should
    // be mapped to vendor and product information but it is only available at
    // kernel level and it can not be queried using class
    // android.hardware.input.InputManager.
    base::string16 device_name;
    base::android::ConvertJavaStringToUTF16(env, devicename, &device_name);
    const size_t name_to_copy =
        std::min(device_name.size(), WebGamepad::idLengthCap - 1);
    memcpy(pad.id, device_name.data(),
           name_to_copy * sizeof(base::string16::value_type));
    pad.id[name_to_copy] = 0;

    base::string16 mapping_name = base::UTF8ToUTF16(mapping ? "standard" : "");
    const size_t mapping_to_copy =
        std::min(mapping_name.size(), WebGamepad::mappingLengthCap - 1);
    memcpy(pad.mapping, mapping_name.data(),
           mapping_to_copy * sizeof(base::string16::value_type));
    pad.mapping[mapping_to_copy] = 0;
  }

  pad.connected = true;
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

}  // namespace device
