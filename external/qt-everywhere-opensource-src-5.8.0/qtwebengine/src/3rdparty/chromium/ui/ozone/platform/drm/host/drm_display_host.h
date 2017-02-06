// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_

#include <memory>

#include "base/macros.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"

namespace ui {

struct DisplaySnapshot_Params;
class DisplaySnapshot;
class GpuThreadAdapter;

class DrmDisplayHost : public GpuThreadObserver {
 public:
  DrmDisplayHost(GpuThreadAdapter* sender,
                 const DisplaySnapshot_Params& params,
                 bool is_dummy);
  ~DrmDisplayHost() override;

  DisplaySnapshot* snapshot() const { return snapshot_.get(); }

  void UpdateDisplaySnapshot(const DisplaySnapshot_Params& params);
  void Configure(const DisplayMode* mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback);
  void GetHDCPState(const GetHDCPStateCallback& callback);
  void SetHDCPState(HDCPState state, const SetHDCPStateCallback& callback);
  void SetColorCorrection(const std::vector<GammaRampRGBEntry>& degamma_lut,
                          const std::vector<GammaRampRGBEntry>& gamma_lut,
                          const std::vector<float>& correction_matrix);

  // Called when the IPC from the GPU process arrives to answer the above
  // commands.
  void OnDisplayConfigured(bool status);
  void OnHDCPStateReceived(bool status, HDCPState state);
  void OnHDCPStateUpdated(bool status);

  // GpuThreadObserver:
  void OnGpuThreadReady() override;
  void OnGpuThreadRetired() override;

 private:
  // Calls all the callbacks with failure.
  void ClearCallbacks();

  GpuThreadAdapter* sender_;  // Not owned.

  std::unique_ptr<DisplaySnapshot> snapshot_;

  // Used during startup to signify that any display configuration should be
  // synchronous and succeed.
  bool is_dummy_;

  ConfigureCallback configure_callback_;
  GetHDCPStateCallback get_hdcp_callback_;
  SetHDCPStateCallback set_hdcp_callback_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
