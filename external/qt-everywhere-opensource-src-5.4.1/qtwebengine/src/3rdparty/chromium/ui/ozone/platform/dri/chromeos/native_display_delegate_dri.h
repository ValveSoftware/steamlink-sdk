// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_DRI_H_
#define UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_DRI_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/display/types/chromeos/native_display_delegate.h"
#include "ui/events/ozone/device/device_event_observer.h"

namespace ui {

class DeviceManager;
class DisplaySnapshotDri;
class DriWrapper;
class ScreenManager;

class NativeDisplayDelegateDri
    : public NativeDisplayDelegate, DeviceEventObserver {
 public:
  NativeDisplayDelegateDri(DriWrapper* dri,
                           ScreenManager* screen_manager,
                           DeviceManager* device_manager);
  virtual ~NativeDisplayDelegateDri();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32_t color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<DisplaySnapshot*> GetDisplays() OVERRIDE;
  virtual void AddMode(const DisplaySnapshot& output,
                       const DisplayMode* mode) OVERRIDE;
  virtual bool Configure(const DisplaySnapshot& output,
                         const DisplayMode* mode,
                         const gfx::Point& origin) OVERRIDE;
  virtual void CreateFrameBuffer(const gfx::Size& size) OVERRIDE;
  virtual bool GetHDCPState(const DisplaySnapshot& output,
                            HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state) OVERRIDE;
  virtual std::vector<ui::ColorCalibrationProfile>
      GetAvailableColorCalibrationProfiles(
          const ui::DisplaySnapshot& output) OVERRIDE;
  virtual bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) OVERRIDE;
  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE;

  // DeviceEventObserver overrides:
  virtual void OnDeviceEvent(const DeviceEvent& event) OVERRIDE;

 private:
  // Notify ScreenManager of all the displays that were present before the
  // update but are gone after the update.
  void NotifyScreenManager(
      const std::vector<DisplaySnapshotDri*>& new_displays,
      const std::vector<DisplaySnapshotDri*>& old_displays) const;

  DriWrapper* dri_;  // Not owned.
  ScreenManager* screen_manager_;  // Not owned.
  DeviceManager* device_manager_;  // Not owned.
  ScopedVector<const DisplayMode> cached_modes_;
  ScopedVector<DisplaySnapshotDri> cached_displays_;
  ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateDri);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_DRI_H_
