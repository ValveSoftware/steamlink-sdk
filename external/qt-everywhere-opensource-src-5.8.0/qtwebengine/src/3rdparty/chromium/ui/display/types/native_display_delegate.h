// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_NATIVE_DISPLAY_DELEGATE_H_
#define UI_DISPLAY_TYPES_NATIVE_DISPLAY_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_types_export.h"

namespace gfx {
class Point;
class Size;
}

namespace ui {
class DisplayMode;
class DisplaySnapshot;

class NativeDisplayObserver;

struct GammaRampRGBEntry;

typedef base::Callback<void(const std::vector<ui::DisplaySnapshot*>&)>
    GetDisplaysCallback;
typedef base::Callback<void(bool)> ConfigureCallback;
typedef base::Callback<void(bool, ui::HDCPState)> GetHDCPStateCallback;
typedef base::Callback<void(bool)> SetHDCPStateCallback;
typedef base::Callback<void(bool)> DisplayControlCallback;

// Interface for classes that perform display configuration actions on behalf
// of DisplayConfigurator.
// Implementations may perform calls asynchronously. In the case of functions
// taking callbacks, the callbacks may be called asynchronously when the results
// are available. The implementations must provide a strong guarantee that the
// callbacks are always called.
class DISPLAY_TYPES_EXPORT NativeDisplayDelegate {
 public:
  virtual ~NativeDisplayDelegate() {}

  virtual void Initialize() = 0;

  // Grabs and refreshes any display server related resources. Must be balanced
  // by a call to UngrabServer().
  virtual void GrabServer() = 0;

  // Released the display server and any resources allocated by GrabServer().
  virtual void UngrabServer() = 0;

  // Take control of the display from any other controlling process.
  virtual void TakeDisplayControl(const DisplayControlCallback& callback) = 0;

  // Let others control the display.
  virtual void RelinquishDisplayControl(
      const DisplayControlCallback& callback) = 0;

  // Flushes all pending requests and waits for replies.
  virtual void SyncWithServer() = 0;

  // Sets the window's background color to |color_argb|.
  virtual void SetBackgroundColor(uint32_t color_argb) = 0;

  // Enables DPMS and forces it to the "on" state.
  virtual void ForceDPMSOn() = 0;

  // Queries for a list of fresh displays and returns them via |callback|.
  // Note the query operation may be expensive and take over 60 milliseconds.
  virtual void GetDisplays(const GetDisplaysCallback& callback) = 0;

  // Adds |mode| to |output|. |mode| must be a valid display mode pointer.
  virtual void AddMode(const ui::DisplaySnapshot& output,
                       const ui::DisplayMode* mode) = 0;

  // Configures the display represented by |output| to use |mode| and positions
  // the display to |origin| in the framebuffer. |mode| can be NULL, which
  // represents disabling the display. The callback will return the status of
  // the operation.
  virtual void Configure(const ui::DisplaySnapshot& output,
                         const ui::DisplayMode* mode,
                         const gfx::Point& origin,
                         const ConfigureCallback& callback) = 0;

  // Called to set the frame buffer (underlying XRR "screen") size.
  virtual void CreateFrameBuffer(const gfx::Size& size) = 0;

  // Gets HDCP state of output.
  virtual void GetHDCPState(const ui::DisplaySnapshot& output,
                            const GetHDCPStateCallback& callback) = 0;

  // Sets HDCP state of output.
  virtual void SetHDCPState(const ui::DisplaySnapshot& output,
                            ui::HDCPState state,
                            const SetHDCPStateCallback& callback) = 0;

  // Gets the available list of color calibrations.
  virtual std::vector<ui::ColorCalibrationProfile>
      GetAvailableColorCalibrationProfiles(
          const ui::DisplaySnapshot& output) = 0;

  // Sets the color calibration of |output| to |new_profile|.
  virtual bool SetColorCalibrationProfile(
      const ui::DisplaySnapshot& output,
      ui::ColorCalibrationProfile new_profile) = 0;

  // Set the gamma tables and corection matrix for the display.
  virtual bool SetColorCorrection(
      const ui::DisplaySnapshot& output,
      const std::vector<GammaRampRGBEntry>& degamma_lut,
      const std::vector<GammaRampRGBEntry>& gamma_lut,
      const std::vector<float>& correction_matrix) = 0;

  virtual void AddObserver(NativeDisplayObserver* observer) = 0;

  virtual void RemoveObserver(NativeDisplayObserver* observer) = 0;
};

}  // namespace ui

#endif  // UI_DISPLAY_TYPES_NATIVE_DISPLAY_DELEGATE_H_
