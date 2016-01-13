// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_MODE_DRI_H_
#define UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_MODE_DRI_H_

#include <stdint.h>
#include <stdlib.h>
#include <xf86drmMode.h>

#include "ui/display/types/chromeos/display_mode.h"

namespace ui {

class DisplayModeDri : public DisplayMode {
 public:
  DisplayModeDri(const drmModeModeInfo& mode);
  virtual ~DisplayModeDri();

  // Native details about this mode. Only used internally in the DRI
  // implementation.
  drmModeModeInfo mode_info() const { return mode_info_; }

 private:
  drmModeModeInfo mode_info_;

  DISALLOW_COPY_AND_ASSIGN(DisplayModeDri);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CHROMEOS_DISPLAY_MODE_DRI_H_
