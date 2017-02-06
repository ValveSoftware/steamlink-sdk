// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"

#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/ozone/platform/drm/host/drm_display_host.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"

namespace ui {

DrmNativeDisplayDelegate::DrmNativeDisplayDelegate(
    DrmDisplayHostManager* display_manager)
    : display_manager_(display_manager) {
}

DrmNativeDisplayDelegate::~DrmNativeDisplayDelegate() {
  display_manager_->RemoveDelegate(this);
}

void DrmNativeDisplayDelegate::OnConfigurationChanged() {
  FOR_EACH_OBSERVER(NativeDisplayObserver, observers_,
                    OnConfigurationChanged());
}

void DrmNativeDisplayDelegate::Initialize() {
  display_manager_->AddDelegate(this);
}

void DrmNativeDisplayDelegate::GrabServer() {
}

void DrmNativeDisplayDelegate::UngrabServer() {
}

void DrmNativeDisplayDelegate::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  display_manager_->TakeDisplayControl(callback);
}

void DrmNativeDisplayDelegate::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  display_manager_->RelinquishDisplayControl(callback);
}

void DrmNativeDisplayDelegate::SyncWithServer() {
}

void DrmNativeDisplayDelegate::SetBackgroundColor(uint32_t color_argb) {
}

void DrmNativeDisplayDelegate::ForceDPMSOn() {
}

void DrmNativeDisplayDelegate::GetDisplays(
    const GetDisplaysCallback& callback) {
  display_manager_->UpdateDisplays(callback);
}

void DrmNativeDisplayDelegate::AddMode(const ui::DisplaySnapshot& output,
                                       const ui::DisplayMode* mode) {
}

void DrmNativeDisplayDelegate::Configure(const ui::DisplaySnapshot& output,
                                         const ui::DisplayMode* mode,
                                         const gfx::Point& origin,
                                         const ConfigureCallback& callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(output.display_id());
  display->Configure(mode, origin, callback);
}

void DrmNativeDisplayDelegate::CreateFrameBuffer(const gfx::Size& size) {
}

void DrmNativeDisplayDelegate::GetHDCPState(
    const ui::DisplaySnapshot& output,
    const GetHDCPStateCallback& callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(output.display_id());
  display->GetHDCPState(callback);
}

void DrmNativeDisplayDelegate::SetHDCPState(
    const ui::DisplaySnapshot& output,
    ui::HDCPState state,
    const SetHDCPStateCallback& callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(output.display_id());
  display->SetHDCPState(state, callback);
}

std::vector<ui::ColorCalibrationProfile>
DrmNativeDisplayDelegate::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  return std::vector<ui::ColorCalibrationProfile>();
}

bool DrmNativeDisplayDelegate::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  NOTIMPLEMENTED();
  return false;
}

bool DrmNativeDisplayDelegate::SetColorCorrection(
    const ui::DisplaySnapshot& output,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DrmDisplayHost* display = display_manager_->GetDisplay(output.display_id());
  display->SetColorCorrection(degamma_lut, gamma_lut, correction_matrix);
  return true;
}

void DrmNativeDisplayDelegate::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DrmNativeDisplayDelegate::RemoveObserver(NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
