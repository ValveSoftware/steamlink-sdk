// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/native_display_delegate_ozone.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/common/display_util.h"

namespace ui {

NativeDisplayDelegateOzone::NativeDisplayDelegateOzone() {
}

NativeDisplayDelegateOzone::~NativeDisplayDelegateOzone() {
}

void NativeDisplayDelegateOzone::Initialize() {
  DisplaySnapshot_Params params;
  if (CreateSnapshotFromCommandLine(&params)) {
    DCHECK_NE(DISPLAY_CONNECTION_TYPE_NONE, params.type);
    displays_.push_back(base::WrapUnique(new DisplaySnapshotProxy(params)));
  }
}

void NativeDisplayDelegateOzone::GrabServer() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::UngrabServer() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

void NativeDisplayDelegateOzone::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

void NativeDisplayDelegateOzone::SyncWithServer() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::SetBackgroundColor(uint32_t color_argb) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::ForceDPMSOn() {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::GetDisplays(
    const GetDisplaysCallback& callback) {
  std::vector<DisplaySnapshot*> displays;
  for (const auto& display : displays_)
    displays.push_back(display.get());
  callback.Run(displays);
}

void NativeDisplayDelegateOzone::AddMode(const ui::DisplaySnapshot& output,
                                         const ui::DisplayMode* mode) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::Configure(const ui::DisplaySnapshot& output,
                                           const ui::DisplayMode* mode,
                                           const gfx::Point& origin,
                                           const ConfigureCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(true);
}

void NativeDisplayDelegateOzone::CreateFrameBuffer(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::GetHDCPState(
    const ui::DisplaySnapshot& output,
    const GetHDCPStateCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false, HDCP_STATE_UNDESIRED);
}

void NativeDisplayDelegateOzone::SetHDCPState(
    const ui::DisplaySnapshot& output,
    ui::HDCPState state,
    const SetHDCPStateCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

std::vector<ui::ColorCalibrationProfile>
NativeDisplayDelegateOzone::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  NOTIMPLEMENTED();
  return std::vector<ui::ColorCalibrationProfile>();
}

bool NativeDisplayDelegateOzone::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  NOTIMPLEMENTED();
  return false;
}

bool NativeDisplayDelegateOzone::SetColorCorrection(
    const ui::DisplaySnapshot& output,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  NOTIMPLEMENTED();
  return false;
}

void NativeDisplayDelegateOzone::AddObserver(NativeDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateOzone::RemoveObserver(
    NativeDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace ui
