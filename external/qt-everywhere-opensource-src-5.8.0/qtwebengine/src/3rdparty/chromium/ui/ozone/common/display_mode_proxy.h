// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_DISPLAY_MODE_PROXY_H_
#define UI_OZONE_COMMON_DISPLAY_MODE_PROXY_H_

#include "base/macros.h"
#include "ui/display/types/display_mode.h"

namespace ui {

struct DisplayMode_Params;

class DisplayModeProxy : public DisplayMode {
 public:
  explicit DisplayModeProxy(const DisplayMode_Params& params);
  ~DisplayModeProxy() override;
  std::unique_ptr<DisplayMode> Clone() const override;

 private:
  DisplayModeProxy(const gfx::Size& size, bool interlaced, float refresh_rate);

  DISALLOW_COPY_AND_ASSIGN(DisplayModeProxy);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_DISPLAY_MODE_PROXY_H_
