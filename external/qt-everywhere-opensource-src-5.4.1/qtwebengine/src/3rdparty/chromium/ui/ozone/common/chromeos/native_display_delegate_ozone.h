// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_CHROMEOS_NATIVE_DISPLAY_DELEGATE_OZONE_H_
#define UI_OZONE_COMMON_CHROMEOS_NATIVE_DISPLAY_DELEGATE_OZONE_H_

#include "base/macros.h"
#include "ui/display/types/chromeos/native_display_delegate.h"

namespace ui {

class NativeDisplayDelegateOzone : public NativeDisplayDelegate {
 public:
  NativeDisplayDelegateOzone();
  virtual ~NativeDisplayDelegateOzone();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32_t color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<ui::DisplaySnapshot*> GetDisplays() OVERRIDE;
  virtual void AddMode(const ui::DisplaySnapshot& output,
                       const ui::DisplayMode* mode) OVERRIDE;
  virtual bool Configure(const ui::DisplaySnapshot& output,
                         const ui::DisplayMode* mode,
                         const gfx::Point& origin) OVERRIDE;
  virtual void CreateFrameBuffer(const gfx::Size& size) OVERRIDE;
  virtual bool GetHDCPState(const ui::DisplaySnapshot& output,
                            ui::HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(const ui::DisplaySnapshot& output,
                            ui::HDCPState state) OVERRIDE;
  virtual std::vector<ui::ColorCalibrationProfile>
      GetAvailableColorCalibrationProfiles(
          const ui::DisplaySnapshot& output) OVERRIDE;
  virtual bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) OVERRIDE;
  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateOzone);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_CHROMEOS_NATIVE_DISPLAY_DELEGATE_OZONE_H_
