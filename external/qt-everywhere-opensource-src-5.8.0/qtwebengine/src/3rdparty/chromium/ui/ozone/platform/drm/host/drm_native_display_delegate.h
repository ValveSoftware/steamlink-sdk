// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

class DrmDisplayHostManager;

class DrmNativeDisplayDelegate : public NativeDisplayDelegate {
 public:
  DrmNativeDisplayDelegate(DrmDisplayHostManager* display_manager);
  ~DrmNativeDisplayDelegate() override;

  void OnConfigurationChanged();

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  void TakeDisplayControl(const DisplayControlCallback& callback) override;
  void RelinquishDisplayControl(
      const DisplayControlCallback& callback) override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  void GetDisplays(const GetDisplaysCallback& callback) override;
  void AddMode(const ui::DisplaySnapshot& output,
               const ui::DisplayMode* mode) override;
  void Configure(const ui::DisplaySnapshot& output,
                 const ui::DisplayMode* mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback) override;
  void CreateFrameBuffer(const gfx::Size& size) override;
  void GetHDCPState(const ui::DisplaySnapshot& output,
                    const GetHDCPStateCallback& callback) override;
  void SetHDCPState(const ui::DisplaySnapshot& output,
                    ui::HDCPState state,
                    const SetHDCPStateCallback& callback) override;
  std::vector<ui::ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      const ui::DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) override;
  bool SetColorCorrection(const ui::DisplaySnapshot& output,
                          const std::vector<GammaRampRGBEntry>& degamma_lut,
                          const std::vector<GammaRampRGBEntry>& gamma_lut,
                          const std::vector<float>& correction_matrix) override;

  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;

 private:
  DrmDisplayHostManager* display_manager_;  // Not owned.

  base::ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DrmNativeDisplayDelegate);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_NATIVE_DISPLAY_DELEGATE_H_
