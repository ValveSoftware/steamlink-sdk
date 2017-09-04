// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"

#include "base/strings/utf_string_conversions.h"

#include "device/vr/android/gvr/gvr_delegate.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

namespace {

void CopyToWebUString(blink::WebUChar* dest,
                      size_t dest_length,
                      base::string16 src) {
  static_assert(sizeof(base::string16::value_type) == sizeof(blink::WebUChar),
                "Mismatched string16/WebUChar size.");

  const size_t str_to_copy = std::min(src.size(), dest_length - 1);
  memcpy(dest, src.data(), str_to_copy * sizeof(base::string16::value_type));
  dest[str_to_copy] = 0;
}

}  // namespace

using namespace blink;

GvrGamepadDataFetcher::Factory::Factory(
    const base::WeakPtr<GvrDelegate>& delegate,
    unsigned int display_id)
    : delegate_(delegate), display_id_(display_id) {}

GvrGamepadDataFetcher::Factory::~Factory() {}

std::unique_ptr<GamepadDataFetcher>
GvrGamepadDataFetcher::Factory::CreateDataFetcher() {
  if (!delegate_)
    return nullptr;
  return base::MakeUnique<GvrGamepadDataFetcher>(delegate_.get(), display_id_);
}

GamepadSource GvrGamepadDataFetcher::Factory::source() {
  return GAMEPAD_SOURCE_GVR;
}

GvrGamepadDataFetcher::GvrGamepadDataFetcher(GvrDelegate* delegate,
                                             unsigned int display_id)
    : display_id_(display_id) {
  gvr::GvrApi* gvr_api = delegate->gvr_api();
  controller_api_.reset(new gvr::ControllerApi());
  int32_t options = gvr::ControllerApi::DefaultOptions();
  options |= GVR_CONTROLLER_ENABLE_GYRO;
  bool success = controller_api_->Init(options, gvr_api->GetContext());
  if (!success)
    controller_api_.reset(nullptr);

  // TODO(bajones): Monitor changes to the controller handedness.
  handedness_ = gvr_api->GetUserPrefs().GetControllerHandedness();
}

GvrGamepadDataFetcher::~GvrGamepadDataFetcher() {}

GamepadSource GvrGamepadDataFetcher::source() {
  return GAMEPAD_SOURCE_GVR;
}

void GvrGamepadDataFetcher::OnAddedToProvider() {
  PauseHint(false);
}

void GvrGamepadDataFetcher::GetGamepadData(bool devices_changed_hint) {
  if (!controller_api_)
    return;

  PadState* state = GetPadState(0);
  if (!state)
    return;

  WebGamepad& pad = state->data;
  if (state->active_state == GAMEPAD_NEWLY_ACTIVE) {
    // This is the first time we've seen this device, so do some one-time
    // initialization
    pad.connected = true;
    CopyToWebUString(pad.id, WebGamepad::idLengthCap,
                     base::UTF8ToUTF16("Daydream Controller"));
    CopyToWebUString(pad.mapping, WebGamepad::mappingLengthCap,
                     base::UTF8ToUTF16(""));
    pad.buttonsLength = 1;
    pad.axesLength = 2;

    pad.displayId = display_id_;

    pad.hand = (handedness_ == GVR_CONTROLLER_RIGHT_HANDED) ? GamepadHandRight
                                                            : GamepadHandLeft;
  }

  controller_state_.Update(*controller_api_);

  pad.timestamp = controller_state_.GetLastOrientationTimestamp();

  if (controller_state_.IsTouching()) {
    gvr_vec2f touch_position = controller_state_.GetTouchPos();
    pad.axes[0] = (touch_position.x * 2.0f) - 1.0f;
    pad.axes[1] = (touch_position.y * 2.0f) - 1.0f;
  } else {
    pad.axes[0] = 0.0f;
    pad.axes[1] = 0.0f;
  }

  pad.buttons[0].touched = controller_state_.IsTouching();
  pad.buttons[0].pressed =
      controller_state_.GetButtonState(GVR_CONTROLLER_BUTTON_CLICK);
  pad.buttons[0].value = pad.buttons[0].pressed ? 1.0f : 0.0f;

  pad.pose.notNull = true;
  pad.pose.hasOrientation = true;
  pad.pose.hasPosition = false;

  gvr_quatf orientation = controller_state_.GetOrientation();
  pad.pose.orientation.notNull = true;
  pad.pose.orientation.x = orientation.qx;
  pad.pose.orientation.y = orientation.qy;
  pad.pose.orientation.z = orientation.qz;
  pad.pose.orientation.w = orientation.qw;

  gvr_vec3f accel = controller_state_.GetAccel();
  pad.pose.linearAcceleration.notNull = true;
  pad.pose.linearAcceleration.x = accel.x;
  pad.pose.linearAcceleration.y = accel.y;
  pad.pose.linearAcceleration.z = accel.z;

  gvr_vec3f gyro = controller_state_.GetGyro();
  pad.pose.angularVelocity.notNull = true;
  pad.pose.angularVelocity.x = gyro.x;
  pad.pose.angularVelocity.y = gyro.y;
  pad.pose.angularVelocity.z = gyro.z;
}

void GvrGamepadDataFetcher::PauseHint(bool paused) {
  if (!controller_api_)
    return;

  if (paused) {
    controller_api_->Pause();
  } else {
    controller_api_->Resume();
  }
}

}  // namespace device
