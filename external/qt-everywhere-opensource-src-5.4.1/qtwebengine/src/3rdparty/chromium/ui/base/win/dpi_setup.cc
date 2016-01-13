// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/dpi_setup.h"

#include "base/command_line.h"
#include "ui/base/layout.h"
#include "ui/gfx/display.h"
#include "ui/gfx/win/dpi.h"

namespace ui {
namespace win {

void InitDeviceScaleFactor() {
  gfx::InitDeviceScaleFactor(gfx::GetDPIScale());
}

}  // namespace win
}  // namespace ui
